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

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include <starpu.h>

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

static struct starpu_task *create_dummy_task(void)
{
	struct starpu_task *task = starpu_task_create();

	task->cl = &dummy_codelet;
	task->cl_arg = NULL;

	return task;
}

int main(int argc, char **argv)
{
	starpu_init(NULL);

	fprintf(stderr, "{ A } -> { B }\n");
	fflush(stderr);

	struct starpu_task *taskA, *taskB;
   starpu_event eventA, eventB;
	
	taskA = create_dummy_task();
	taskB = create_dummy_task();

	starpu_task_submit(taskA, &eventA);
	starpu_task_submit_ex(taskB, 1, &eventA, &eventB);
   starpu_event_release(eventA);

	starpu_event_wait_and_release(eventB);

	fprintf(stderr, "{ C, D, E, F } -> { G }\n");

   struct starpu_task *tasksCDEF[4], *taskG;
   starpu_event eventCDEF, eventG;

	tasksCDEF[0] = create_dummy_task();
	tasksCDEF[1] = create_dummy_task();
	tasksCDEF[2] = create_dummy_task();
	tasksCDEF[3] = create_dummy_task();
	taskG = create_dummy_task();

	starpu_task_submit_all(4, tasksCDEF, &eventCDEF);

	starpu_task_submit_ex(taskG, 1, &eventCDEF, &eventG);

   starpu_event_release(eventCDEF);
	starpu_event_wait_and_release(eventG);

	fprintf(stderr, "{ H, I } -> { J, K, L }\n");
	
	struct starpu_task *taskH, *taskI, *taskJ, *taskK, *taskL;
   starpu_event eventH, eventI, eventJ, eventK, eventL;

	taskH = create_dummy_task();
	taskI = create_dummy_task();
	taskJ = create_dummy_task();
	taskK = create_dummy_task();
	taskL = create_dummy_task();

	starpu_task_submit(taskH, &eventH);
	starpu_task_submit(taskI, &eventI);

   starpu_event deps2[] = {eventH, eventI};

	starpu_task_submit_ex(taskJ, 2, deps2, &eventJ);
	starpu_task_submit_ex(taskK, 2, deps2, &eventK);
	starpu_task_submit_ex(taskL, 2, deps2, &eventL);

   starpu_event_release_all(2, deps2);

   starpu_event deps3[] = {eventJ, eventK, eventL};
   starpu_event_wait_and_release_all(3, deps3);


	starpu_shutdown();

	return 0;
}
