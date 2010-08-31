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

#include <starpu.h>
#include <common/config.h>
#include <core/jobs.h>

struct wrapper_func_args {
	void (*func)(void *);
	void *arg;
};

static void wrapper_func(void *buffers[] __attribute__ ((unused)), void *_args)
{
	struct wrapper_func_args *args = _args;
	args->func(args->arg);
}

static struct starpu_perfmodel_t wrapper_model = {
	.type = STARPU_HISTORY_BASED,
	.symbol = "_wrapper_model"
};


/* execute func(arg) on each worker that matches the "where" flag */
void starpu_execute_on_each_worker(void (*func)(void *), void *arg, uint32_t where)
{
	int ret;
	unsigned worker;
	unsigned nworkers = starpu_worker_get_count();
	starpu_event events[STARPU_NMAXWORKERS];

	/* create a wrapper codelet */
	struct starpu_codelet_t wrapper_cl = {
		.where = where,
		.cuda_func = wrapper_func,
		.cpu_func = wrapper_func,
		.opencl_func = wrapper_func,
		/* XXX we do not handle Cell .. */
		.nbuffers = 0,
		.model = &wrapper_model
	};

	struct wrapper_func_args args = {
		.func = func,
		.arg = arg
	};

	for (worker = 0; worker < nworkers; worker++)
	{
      struct starpu_task * task;
		task = starpu_task_create();

		task->cl = &wrapper_cl;
		task->cl_arg = &args;

		task->execute_on_a_specific_worker = 1;
		task->workerid = worker;

		task->destroy = 1;

#ifdef STARPU_USE_FXT
		_starpu_exclude_task_from_dag(task);
#endif

		ret = starpu_task_submit(task, &events[worker]);
		if (ret == -ENODEV)
		{
			/* if the worker is not able to execute this tasks, we
			 * don't insist as this means the worker is not
			 * designated by the "where" bitmap */
			events[worker] = NULL;
		}
	}

	for (worker = 0; worker < nworkers; worker++)
	{
		if (events[worker] != NULL)
		{
			ret = starpu_event_wait(events[worker]);
			STARPU_ASSERT(!ret);
         starpu_event_release(events[worker]);
		}
	}
}
