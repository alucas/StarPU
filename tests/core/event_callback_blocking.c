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
#include <unistd.h>

void callback(void*);

volatile int ret = 0;

static void dummy_func(void *descr[] __attribute__ ((unused)), void *arg __attribute__ ((unused)))
{
}

static starpu_codelet dummy_codelet = 
{
	.where = STARPU_CPU|STARPU_CUDA,
	.cpu_func = dummy_func,
	.cuda_func = dummy_func,
	.model = NULL,
	.nbuffers = 0
};

int main() {

   starpu_event event, event2;

	starpu_init(NULL);

   struct starpu_task *task = starpu_task_create();
   task->cl = &dummy_codelet;
	task->cl_arg = NULL;

   starpu_task_submit(task, &event);

   assert(event != NULL);

   event2 = starpu_event_create();

   starpu_event_callback_add(event, &callback, event2, NULL);

   starpu_event_trigger(event2);

   starpu_event_wait_and_release(event);

	starpu_shutdown();

   return (ret != 0 ? 0 : -1);
}

void callback(void*data) {
   starpu_event event = (starpu_event)data;

   /* This shouldn't work as we are not allowed to wait in a callback */
   ret = starpu_event_wait(event);
}

