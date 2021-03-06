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

/* Stack queues, ready for use by schedulers */

#ifndef __STACK_QUEUES_H__
#define __STACK_QUEUES_H__

#include <starpu.h>
#include <common/config.h>
#include <core/jobs.h>

struct starpu_stack_jobq_s {
	/* the actual list */
	starpu_job_list_t jobq;

	/* the number of tasks currently in the queue */
	unsigned njobs;

	/* the number of tasks that were processed */
	unsigned nprocessed;

	/* only meaningful if the queue is only used by a single worker */
	double exp_start;
	double exp_end;
	double exp_len;
};

struct starpu_stack_jobq_s *_starpu_create_stack(void);

void _starpu_stack_push_task(struct starpu_stack_jobq_s *stack, pthread_mutex_t *sched_mutex, pthread_cond_t *sched_cond, starpu_job_t task);
void _starpu_stack_push_prio_task(struct starpu_stack_jobq_s *stack, pthread_mutex_t *sched_mutex, pthread_cond_t *sched_cond, starpu_job_t task);

starpu_job_t _starpu_stack_pop_task(struct starpu_stack_jobq_s *stack, pthread_mutex_t *sched_mutex);

void _starpu_init_stack_queues_mechanisms(void);


unsigned _starpu_get_stack_njobs(struct starpu_stack_jobq_s *stack);
unsigned _starpu_get_stack_nprocessed(struct starpu_stack_jobq_s *stack);


#endif // __STACK_QUEUES_H__
