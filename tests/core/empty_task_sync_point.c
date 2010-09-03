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

int main(int argc, char **argv)
{
	starpu_init(NULL);

   starpu_event eventABC[3], eventD, eventEF[2];

	/* {A,B,C} -> D -> {E,F}, D is empty */
	struct starpu_task *taskA = starpu_task_create();
	taskA->cl = &dummy_codelet;
	
	struct starpu_task *taskB = starpu_task_create();
	taskB->cl = &dummy_codelet;
	
	struct starpu_task *taskC = starpu_task_create();
	taskC->cl = &dummy_codelet;

	struct starpu_task *taskD = starpu_task_create();
	taskD->cl = NULL;

	struct starpu_task *taskE = starpu_task_create();
	taskE->cl = &dummy_codelet;

	struct starpu_task *taskF = starpu_task_create();
	taskF->cl = &dummy_codelet;

	starpu_task_submit(taskA, &eventABC[0]);
	starpu_task_submit(taskB, &eventABC[1]);
	starpu_task_submit(taskC, &eventABC[2]);

	starpu_task_submit_ex(taskD, 3, eventABC, &eventD);

	starpu_task_submit_ex(taskE, 1, &eventD, &eventEF[0]);
	starpu_task_submit_ex(taskF, 1, &eventD, &eventEF[1]);

   starpu_event_wait_all(2, eventEF);

   starpu_event_release_all(3, eventABC);
   starpu_event_release(eventD);
   starpu_event_release_all(2, eventEF);

	starpu_shutdown();

	return 0;
}
