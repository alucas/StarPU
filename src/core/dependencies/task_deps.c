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
#include <common/utils.h>
#include <core/dependencies/tags.h>
#include <core/dependencies/htable.h>
#include <core/jobs.h>
#include <core/task.h>
#include <core/sched_policy.h>
#include <core/dependencies/data_concurrency.h>

static starpu_cg_t *create_cg_task(unsigned ntags, starpu_job_t j)
{
	starpu_cg_t *cg = malloc(sizeof(starpu_cg_t));
	STARPU_ASSERT(cg);

	cg->ntags = ntags;
	cg->remaining = ntags;
	cg->cg_type = STARPU_CG_TASK;

	cg->succ.job = j;
	j->job_successors.ndeps++;

	return cg;
}

/* the job lock must be taken */
static void _starpu_task_add_succ(starpu_job_t j, starpu_cg_t *cg)
{
	STARPU_ASSERT(j);

	_starpu_add_successor_to_cg_list(&j->job_successors, cg);

	if (j->terminated) {
		/* the task was already completed sooner */
		_starpu_notify_cg(cg);
	}
}

void _starpu_notify_task_dependencies(starpu_job_t j)
{
	_starpu_notify_cg_list(&j->job_successors);
}

/* task depends on the tasks in task array */
void starpu_task_declare_deps_array(struct starpu_task *task, unsigned num_events, starpu_event *events)
{
	if (num_events == 0)
		return;

	starpu_job_t job;

	job = _starpu_get_job_associated_to_task(task);

   //STARPU_TRACE_TASK_DEPS(dep_job, job);
   _starpu_trigger_events_register(&job->trigger, num_events, events);
}
