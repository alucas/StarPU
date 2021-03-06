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

#ifndef __JOBS_H__
#define __JOBS_H__

#include <starpu.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include <common/config.h>
#include <common/timing.h>
#include <common/list.h>
#include <common/fxt.h>
#include <core/trigger.h>
#include <datawizard/datawizard.h>
#include <core/perfmodel/perfmodel.h>
#include <core/errorcheck.h>

#ifdef STARPU_USE_CUDA
#include <cuda.h>
#endif

struct starpu_worker_s;

/* codelet function */
typedef void (*cl_func)(void **, void *);
typedef void (*callback)(void *);

#define STARPU_CPU_MAY_PERFORM(j)	((j)->task->cl->where & STARPU_CPU)
#define STARPU_CUDA_MAY_PERFORM(j)      ((j)->task->cl->where & STARPU_CUDA)
#define STARPU_SPU_MAY_PERFORM(j)	((j)->task->cl->where & STARPU_SPU)
#define STARPU_GORDON_MAY_PERFORM(j)	((j)->task->cl->where & STARPU_GORDON)
#define STARPU_OPENCL_MAY_PERFORM(j)	((j)->task->cl->where & STARPU_OPENCL)

/* a job is the internal representation of a task */
LIST_TYPE(starpu_job,
	struct starpu_task *task;

   starpu_event event;

   struct starpu_trigger_t trigger;
   volatile int ready;

	pthread_mutex_t sync_mutex;
	pthread_cond_t sync_cond;

	struct starpu_buffer_descr_t ordered_buffers[STARPU_NMAXBUFS];
	
	starpu_mem_chunk_t scratch_memchunks[STARPU_NMAXBUFS];

	unsigned footprint_is_computed;
	uint32_t footprint;

	unsigned scheduled;
	unsigned submitted;
	unsigned terminated;

#ifdef STARPU_USE_FXT
	unsigned long job_id;
        unsigned exclude_from_dag;
        const char *model_name;
#endif
);

starpu_job_t __attribute__((malloc)) _starpu_job_create(struct starpu_task *task);
void _starpu_job_destroy(starpu_job_t j);

#ifdef STARPU_USE_FXT
void _starpu_exclude_task_from_dag(struct starpu_task *task);
#endif

/*
 *	Schedule a job if it has no dependency left
 * Returns:
 *    - 1 if the task is scheduled
 *    - 0 if the task isn't scheduled (some dependencies remain)
 */
unsigned _starpu_may_schedule(starpu_job_t j);


//FIXME this must not be exported anymore ... 
//starpu_job_t _starpu_job_create(struct starpu_task *task);
void _starpu_handle_job_termination(starpu_job_t j, unsigned job_is_already_locked);
size_t _starpu_job_get_data_size(starpu_job_t j);

starpu_job_t _starpu_pop_local_task(struct starpu_worker_s *worker);
int _starpu_push_local_task(struct starpu_worker_s *worker, starpu_job_t j);

const char *_starpu_get_model_name(starpu_job_t j);

#endif // __JOBS_H__
