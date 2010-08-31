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
#include <core/jobs.h>
#include <core/event.h>
#include <core/task.h>

/* task depends on given events */
void starpu_task_declare_deps_array(struct starpu_task *task, unsigned num_events, starpu_event *events)
{
	if (num_events == 0)
		return;

	starpu_job_t job;

	job = _starpu_get_job_associated_to_task(task);

   //STARPU_TRACE_TASK_DEPS(dep_job, job);
   _starpu_trigger_events_register(&job->trigger, num_events, events);
}
