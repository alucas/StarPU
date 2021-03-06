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
#include <starpu_event.h>
#include <core/event.h>
#include <core/jobs.h>
#include <core/task.h>
#include <core/workers.h>
#include <core/dependencies/data_concurrency.h>
#include <common/config.h>
#include <common/utils.h>

void _starpu_job_trigger_callback(void*);

size_t _starpu_job_get_data_size(starpu_job_t j)
{
	size_t size = 0;

	struct starpu_task *task = j->task;

	unsigned nbuffers = task->cl->nbuffers;

	unsigned buffer;
	for (buffer = 0; buffer < nbuffers; buffer++)
	{
		starpu_data_handle handle = task->buffers[buffer].handle;
		size += _starpu_data_get_size(handle);
	}

	return size;
}

#ifdef STARPU_USE_FXT
/* we need to identify each task to generate the DAG. */
static unsigned long job_cnt = 0;

void _starpu_exclude_task_from_dag(struct starpu_task *task)
{
	starpu_job_t j = _starpu_get_job_associated_to_task(task);

	j->exclude_from_dag = 1;
}
#endif

/* create an internal starpu_job_t structure to encapsulate the task */
starpu_job_t __attribute__((malloc)) _starpu_job_create(struct starpu_task *task)
{
	starpu_job_t job;

	job = starpu_job_new();

	job->task = task;
   job->event = _starpu_event_create();
   _starpu_event_retain_private(job->event);

   _starpu_trigger_init(&job->trigger, &_starpu_job_trigger_callback, job, NULL);
   job->ready = 0;

	job->footprint_is_computed = 0;
	job->submitted = 0;
	job->scheduled = 0;
	job->terminated = 0;

#ifdef STARPU_USE_FXT
	job->job_id = STARPU_ATOMIC_ADD(&job_cnt, 1);
	/* display all tasks by default */
	job->exclude_from_dag = 0;
        job->model_name = NULL;
#endif

	PTHREAD_MUTEX_INIT(&job->sync_mutex, NULL);
	PTHREAD_COND_INIT(&job->sync_cond, NULL);

	return job;
}

void _starpu_job_destroy(starpu_job_t j)
{
	PTHREAD_COND_DESTROY(&j->sync_cond);
	PTHREAD_MUTEX_DESTROY(&j->sync_mutex);

   _starpu_event_release_private(j->event);
	starpu_job_delete(j);
}

void _starpu_handle_job_termination(starpu_job_t j, unsigned job_is_already_locked)
{
	struct starpu_task *task = j->task;

	if (!job_is_already_locked)
		PTHREAD_MUTEX_LOCK(&j->sync_mutex);

	task->status = STARPU_TASK_FINISHED;

	j->submitted = 0;

	/* We must have set the j->terminated flag early, so that it is
	 * possible to express task dependencies within the callback
	 * function. A value of 1 means that the codelet was executed but that
	 * the callback is not done yet. */
	j->terminated = 1;

	if (!job_is_already_locked)
		PTHREAD_MUTEX_UNLOCK(&j->sync_mutex);

	if (task->callback_func)
	{
		/* so that we can check whether we are doing blocking calls
		 * within the callback */
		_starpu_set_local_worker_status(STATUS_CALLBACK);
		
		/* Perhaps we have nested callbacks (eg. with chains of empty
		 * tasks). So we store the current task and we will restore it
		 * later. */
		struct starpu_task *current_task = starpu_get_current_task();

		_starpu_set_current_task(task);

		STARPU_TRACE_START_CALLBACK(j);
		task->callback_func(task->callback_arg);
		STARPU_TRACE_END_CALLBACK(j);
		
		_starpu_set_current_task(current_task);

		_starpu_set_local_worker_status(STATUS_UNKNOWN);
	}

	_starpu_sched_post_exec_hook(task);

	STARPU_TRACE_TASK_DONE(j);

	/* NB: we do not save those values before the callback, in case the
	 * application changes some parameters eventually (eg. a task may not
	 * be generated if the application is terminated). */
	int destroy = task->destroy;
	int regenerate = task->regenerate;

	/* in case there are dependencies, wake up the proper tasks */
   if (!regenerate)
      _starpu_event_complete(j->event);

	if (regenerate)
	{
		STARPU_ASSERT(!destroy && !task->synchronous);

		/* We reuse the same job structure */
		int ret = _starpu_submit_job(j, 1);
		STARPU_ASSERT(!ret);
	}	
	else {
		/* The task is no longer used so we release it.
		 * In case the job was already locked by the caller, it is
       * its responsability to destroy the task.
		 * */
		if (!job_is_already_locked && destroy)
			starpu_task_destroy(task);

		_starpu_decrement_nsubmitted_tasks();
	}

}

/*
 *	Schedule a job if it has no dependency left
 * Returns:
 *    - 1 if the task is scheduled
 *    - 0 if the task isn't scheduled (some dependencies remain)
 */
unsigned _starpu_may_schedule(starpu_job_t j)
{

   PTHREAD_MUTEX_LOCK(&j->sync_mutex);

   if (j->scheduled) {
      PTHREAD_MUTEX_UNLOCK(&j->sync_mutex);
      return 0;
   }

   /* Check events dependencies */
   if (!j->ready) {
      PTHREAD_MUTEX_UNLOCK(&j->sync_mutex);
      return 0;
   }

	/* Check data dependencies */
	if (_starpu_submit_job_enforce_data_deps(j)) {
      PTHREAD_MUTEX_UNLOCK(&j->sync_mutex);
		return 0;
   }

   /* We can schedule the task */
	unsigned ret;
   ret = _starpu_push_task(j, 1);
   if (ret) {
      PTHREAD_MUTEX_UNLOCK(&j->sync_mutex);
		return 0;
   }

   j->scheduled = 1;

   PTHREAD_MUTEX_UNLOCK(&j->sync_mutex);

	return 1;
}

struct starpu_job_s *_starpu_pop_local_task(struct starpu_worker_s *worker)
{
	struct starpu_job_s *j = NULL;

	PTHREAD_MUTEX_LOCK(&worker->local_jobs_mutex);

	if (!starpu_job_list_empty(worker->local_jobs))
		j = starpu_job_list_pop_back(worker->local_jobs);

	PTHREAD_MUTEX_UNLOCK(&worker->local_jobs_mutex);

	return j;
}

int _starpu_push_local_task(struct starpu_worker_s *worker, struct starpu_job_s *j)
{
	/* Check that the worker is able to execute the task ! */
	STARPU_ASSERT(j->task && j->task->cl);
	if (STARPU_UNLIKELY(!(worker->worker_mask & j->task->cl->where)))
		return -ENODEV;

	PTHREAD_MUTEX_LOCK(&worker->local_jobs_mutex);

	starpu_job_list_push_front(worker->local_jobs, j);

	PTHREAD_MUTEX_UNLOCK(&worker->local_jobs_mutex);

#ifndef STARPU_NON_BLOCKING_DRIVERS
	/* XXX that's a bit excessive ... */
	_starpu_wake_all_blocked_workers_on_node(worker->memory_node);
#endif

	return 0;
}

const char *_starpu_get_model_name(starpu_job_t j) {
        struct starpu_task *task = j->task;
        if (task && task->cl
            && task->cl->model
            && task->cl->model->symbol)
                return task->cl->model->symbol;
#ifdef STARPU_USE_FXT
        else {
                return j->model_name;
        }
#endif
        return NULL;
}

/* This function is called when the job trigger is triggered.
 * That is, when all dependencies are fulfilled
 */
void _starpu_job_trigger_callback(void * data) {
   starpu_job_t job = (starpu_job_t)data;

   job->ready = 1;

   _starpu_may_schedule(job);
}
