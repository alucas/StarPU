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

#define N	4

int main(int argc, char **argv)
{
	int i, ret;

	starpu_init(NULL);

	starpu_event user_event, event;
   user_event = starpu_event_create();
   starpu_event_retain(user_event);
   event = user_event;

	for (i = 0; i < N; i++)
	{
      struct starpu_task * task;
		task = starpu_task_create();
		task->cl = NULL;

      starpu_task_declare_deps_array(task, 1, &event);
      starpu_event_release(event);

      ret = starpu_task_submit(task, &event);
      STARPU_ASSERT(!ret);
	}

   starpu_event_trigger(user_event);

	starpu_event_wait(user_event);
   starpu_event_release(user_event);

	starpu_shutdown();

	return 0;
}
