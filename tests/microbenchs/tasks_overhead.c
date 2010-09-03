/*
 * StarPU
 * Copyright (C) Université Bordeaux 1, CNRS 2008-2010 (see AUTHORS file)
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

#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#include <starpu.h>

starpu_data_handle data_handles[8];
float *buffers[8];

static unsigned ntasks = 65536;
static unsigned nbuffers = 0;


static void dummy_func(void *descr[] __attribute__ ((unused)), void *arg __attribute__ ((unused)))
{
}

static starpu_codelet dummy_codelet = 
{
	.where = STARPU_CPU|STARPU_CUDA|STARPU_OPENCL,
	.cpu_func = dummy_func,
	.cuda_func = dummy_func,
	.opencl_func = dummy_func,
	.model = NULL,
	.nbuffers = 0
};

void inject_one_task(void)
{
	struct starpu_task *task = starpu_task_create();

	task->cl = &dummy_codelet;
	task->cl_arg = NULL;
	task->callback_func = NULL;
	task->synchronous = 1;

	starpu_task_submit(task, NULL);
}

static void parse_args(int argc, char **argv)
{
	int c;
	while ((c = getopt(argc, argv, "i:b:h")) != -1)
	switch(c) {
		case 'i':
			ntasks = atoi(optarg);
			break;
		case 'b':
			nbuffers = atoi(optarg);
			dummy_codelet.nbuffers = nbuffers;
			break;
		case 'h':
			fprintf(stderr, "Usage: %s [-i ntasks] [-b nbuffers] [-h]\n", argv[0]);
			break;
	}
}

int main(int argc, char **argv)
{
	unsigned i;

	double timing_submit;
	struct timeval start_submit;
	struct timeval end_submit;

	double timing_exec;
	struct timeval start_exec;
	struct timeval end_exec;
   struct starpu_task *tasks;
   starpu_event *events;

	parse_args(argc, argv);

	unsigned buffer;
	for (buffer = 0; buffer < nbuffers; buffer++)
	{
		buffers[buffer] = malloc(16*sizeof(float));
		starpu_vector_data_register(&data_handles[buffer], 0, (uintptr_t)buffers[buffer], 16, sizeof(float));
	}

	starpu_init(NULL);

	fprintf(stderr, "#tasks : %d\n#buffers : %d\n", ntasks, nbuffers);

	/* submit tasks */
	tasks = malloc(ntasks*sizeof(struct starpu_task));
	events = malloc((ntasks+1)*sizeof(starpu_event));

   events[0] = starpu_event_create();

	gettimeofday(&start_submit, NULL);
	for (i = 0; i < ntasks; i++)
	{
		tasks[i].callback_func = NULL;
		tasks[i].cl = &dummy_codelet;
		tasks[i].cl_arg = NULL;
		tasks[i].synchronous = 0;

		/* we have 8 buffers at most */
		for (buffer = 0; buffer < nbuffers; buffer++)
		{
			tasks[i].buffers[buffer].handle = data_handles[buffer];
			tasks[i].buffers[buffer].mode = STARPU_RW;
		}
	}

	gettimeofday(&start_submit, NULL);
	for (i = 0; i < ntasks; i++) {
		starpu_task_submit_ex(&tasks[i], 1, &events[i], &events[i+1]);
   }

	/* Trigger the first event */
   starpu_event_trigger(events[0]);

	gettimeofday(&end_submit, NULL);

	/* wait for the execution of the tasks */
	gettimeofday(&start_exec, NULL);
	starpu_event_wait(events[ntasks]);
	gettimeofday(&end_exec, NULL);

	timing_submit = (double)((end_submit.tv_sec - start_submit.tv_sec)*1000000 + (end_submit.tv_usec - start_submit.tv_usec));
	timing_exec = (double)((end_exec.tv_sec - start_exec.tv_sec)*1000000 + (end_exec.tv_usec - start_exec.tv_usec));

	fprintf(stderr, "Total submit: %lf secs\n", timing_submit/1000000);
	fprintf(stderr, "Per task submit: %lf usecs\n", timing_submit/ntasks);
	fprintf(stderr, "\n");
	fprintf(stderr, "Total execution: %lf secs\n", timing_exec/1000000);
	fprintf(stderr, "Per task execution: %lf usecs\n", timing_exec/ntasks);
	fprintf(stderr, "\n");
	fprintf(stderr, "Total: %lf secs\n", (timing_submit+timing_exec)/1000000);
	fprintf(stderr, "Per task: %lf usecs\n", (timing_submit+timing_exec)/ntasks);

	starpu_shutdown();

	return 0;
}
