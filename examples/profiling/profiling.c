/*
 * StarPU
 * Copyright (C) INRIA 2008-2010 (see AUTHORS file)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU Lesser General Public License in COPYING.LGPL for more details.
 */

#include <starpu.h>
#include <starpu_profiling.h>
#include <assert.h>

static unsigned niter = 50;

void sleep_codelet(__attribute__ ((unused)) void *descr[],
			__attribute__ ((unused)) void *_args)
{
	usleep(1000);
}

int main(int argc, char **argv)
{
	if (argc == 2)
		niter = atoi(argv[1]);

	starpu_init(NULL);

	/* Enable profiling */
	starpu_enable_profiling();

	starpu_codelet cl =
	{
		.where = STARPU_CPU|STARPU_CUDA|STARPU_OPENCL,
		.cpu_func = sleep_codelet,
		.cuda_func = sleep_codelet,
		.opencl_func = sleep_codelet,
		.nbuffers = 0
	};

	struct starpu_task **tasks = malloc(niter*sizeof(struct starpu_task *));
	assert(tasks);

	unsigned i;
	for (i = 0; i < niter; i++)
	{
		struct starpu_task *task = starpu_task_create();

		task->cl = &cl;

		/* We will destroy the task structure by hand so that we can
		 * query the profiling info before the task is destroyed. */
		task->destroy = 0;
		
		tasks[i] = task;

		int ret = starpu_task_submit(task);
		if (STARPU_UNLIKELY(ret == -ENODEV))
		{
			fprintf(stderr, "No worker may execute this task\n");
			exit(0);
		}
	}

	starpu_task_wait_for_all();

	int64_t delay_sum = 0;
	int64_t length_sum = 0;

	for (i = 0; i < niter; i++)
	{
		struct starpu_task *task = tasks[i];
		struct starpu_task_profiling_info *info = task->profiling_info;

		/* How much time did it take before the task started ? */
		int64_t delay = (info->start_time - info->submit_time);
		delay_sum += delay;

		/* How long was the task execution ? */
		int64_t length = (info->end_time - info->start_time);
		length_sum += length;

		/* We don't need the task structure anymore */
		starpu_task_destroy(task);
	}

	free(tasks);

	fprintf(stderr, "Avg. delay : %2.2f us\n", ((double)delay_sum)/niter);
	fprintf(stderr, "Avg. length : %2.2f us\n", ((double)length_sum)/niter);

	starpu_shutdown();

	return 0;
}
