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
#include <starpu_profiling.h>
#include <core/workers.h>
#include <core/jobs.h>
#include <core/event.h>
#include <core/trigger.h>
#include <core/task.h>
#include <common/config.h>
#include <common/utils.h>
#include <profiling/profiling.h>
#include <profiling/bound.h>

/* XXX this should be reinitialized when StarPU is shutdown (or we should make
 * sure that no task remains !) */
/* TODO we could make this hierarchical to avoid contention ? */
static pthread_cond_t submitted_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t submitted_mutex = PTHREAD_MUTEX_INITIALIZER;
static long int nsubmitted = 0;

static void _starpu_increment_nsubmitted_tasks(void);

/* This key stores the task currently handled by the thread, note that we
 * cannot use the worker structure to store that information because it is
 * possible that we have a task with a NULL codelet, which means its callback
 * could be executed by a user thread as well. */
static pthread_key_t current_task_key;

void starpu_task_init(struct starpu_task *task)
{
	STARPU_ASSERT(task);

	task->cl = NULL;
	task->cl_arg = NULL;
	task->cl_arg_size = 0;

	task->callback_func = NULL;
	task->callback_arg = NULL;

	task->priority = STARPU_DEFAULT_PRIO;
	task->synchronous = 0;

	task->execute_on_a_specific_worker = 0;

	/* by default, we do not let StarPU free the task structure since
	 * starpu_task_init is likely to be used only for statically allocated
	 * tasks */
	task->destroy = 0;

	task->regenerate = 0;

	task->status = STARPU_TASK_INVALID;

	task->predicted = -1.0;

	task->starpu_private = NULL;
}

/* Free all the ressources allocated for a task, without deallocating the task
 * structure itself (this is required for statically allocated tasks). */
void starpu_task_deinit(struct starpu_task *task)
{
	STARPU_ASSERT(task);

	starpu_job_t j = (struct starpu_job_s *)task->starpu_private;

	if (j)
		_starpu_job_destroy(j);
}

struct starpu_task * __attribute__((malloc)) starpu_task_create(void)
{
	struct starpu_task *task;

	task = calloc(1, sizeof(struct starpu_task));
	STARPU_ASSERT(task);

	starpu_task_init(task);

	/* Dynamically allocated tasks are destroyed by default */
	task->destroy = 1;

	return task;
}

/* Free the ressource allocated during starpu_task_create. This function can be
 * called automatically after the execution of a task by setting the "destroy"
 * flag of the starpu_task structure (default behaviour). Calling this function
 * on a statically allocated task results in an undefined behaviour. */
void starpu_task_destroy(struct starpu_task *task)
{
	STARPU_ASSERT(task);

   /* If starpu_task_destroy is called in a callback, we just set the destroy
      flag. The task will be destroyed after the callback returns */
   if (task == starpu_get_current_task()
       && _starpu_get_local_worker_status() == STATUS_CALLBACK) {

      task->destroy = 1;

   } else {

      starpu_task_deinit(task);

      /* TODO handle the case of non terminated tasks -> return -EINVAL */
	
      free(task);
   }
}

starpu_job_t _starpu_get_job_associated_to_task(struct starpu_task *task)
{
	STARPU_ASSERT(task);

	if (!task->starpu_private)
	{
		starpu_job_t j = _starpu_job_create(task);
		task->starpu_private = j;
	}

	return (struct starpu_job_s *)task->starpu_private;
}

/* NB in case we have a regenerable task, it is possible that the job was
 * already counted. */
int _starpu_submit_job(starpu_job_t j, unsigned do_not_increment_nsubmitted)
{
   if (starpu_event_profiling_enabled(j->event)) {
      struct timespec submit_time;
      starpu_clock_gettime(&submit_time);
      _starpu_event_profiling_submit_time_set(j->event, &submit_time);
   }

	j->terminated = 0;

	if (!do_not_increment_nsubmitted)
		_starpu_increment_nsubmitted_tasks();

	j->submitted = 1;

   /* Enable job trigger. From now, no new dependency can be added */
   _starpu_trigger_enable(&j->trigger);

	return 0;
}

/* application should submit new tasks to StarPU through this function */
int starpu_task_submit(struct starpu_task *task, starpu_event *event)
{
	int ret;
	unsigned is_sync = task->synchronous;

	if (is_sync)
	{
		/* Perhaps it is not possible to submit a synchronous
		 * (blocking) task */
		if (STARPU_UNLIKELY(!_starpu_worker_may_perform_blocking_calls()))
			return -EDEADLK;
	}

	STARPU_ASSERT(task);

	if (task->cl)
	{
		uint32_t where = task->cl->where;
		if (!_starpu_worker_exists(where))
			return -ENODEV;

		/* In case we require that a task should be explicitely
		 * executed on a specific worker, we make sure that the worker
		 * is able to execute this task.  */
		if (task->execute_on_a_specific_worker 
			&& !_starpu_worker_may_execute_task(task->workerid, where))
			return -ENODEV;

		_starpu_detect_implicit_data_deps(task);
	}

	/* The task is considered as block until we are sure there remains not
	 * dependency. */
	task->status = STARPU_TASK_BLOCKED;

	/* internally, StarPU manipulates a starpu_job_t which is a wrapper around a
	* task structure, it is possible that this job structure was already
	* allocated, for instance to enforce task depenencies. */
	starpu_job_t j;

	if (!task->starpu_private)
	{
		j = _starpu_job_create(task);
		task->starpu_private = j;
	}
	else {
		j = (struct starpu_job_s *)task->starpu_private;
	}

	/* notify bound computation of a new task */
	_starpu_bound_record(j);

   if (event != NULL) {
      *event = j->event;
      starpu_event_retain(j->event);
   }

   if (is_sync)
      _starpu_event_retain_private(j->event);

   /* We need to keep an event reference because after _starpu_submit_job,
    * j may be destroyed */
   starpu_event ev = NULL;
   if (is_sync) {
      ev = j->event;
      _starpu_event_retain_private(ev);
   }

	ret = _starpu_submit_job(j, 0);

	if (is_sync) {
		starpu_event_wait(ev);
      _starpu_event_release_private(ev);
   }

	return ret;
}

int starpu_task_submit_all(int num_tasks, struct starpu_task **tasks, starpu_event *event) {
   starpu_event events[num_tasks];
   int i,ret;

   if (event != NULL) {
      for (i=0; i<num_tasks; i++) {
         ret = starpu_task_submit(tasks[i], &events[i]);
         if (ret)
            return ret;
      }
      *event = starpu_event_group_create(num_tasks, events);
      starpu_event_release_all(num_tasks, events);
   }
   else {
      for (i=0; i<num_tasks; i++) {
         ret = starpu_task_submit(tasks[i], NULL);
         if (ret)
            return ret;
      }
   }

   return 0;
}

int starpu_task_submit_ex(struct starpu_task *task, int num_events, starpu_event *events, starpu_event *event) {
   starpu_task_declare_deps_array(task, num_events, events);
   return starpu_task_submit(task, event);
}

int starpu_task_submit_all_ex(int num_tasks, struct starpu_task **tasks, int num_events, starpu_event *pevents, starpu_event *event) {
   starpu_event events[num_tasks];
   int i,ret;

   if (event != NULL) {
      for (i=0; i<num_tasks; i++) {
         ret = starpu_task_submit_ex(tasks[i], num_events, pevents, &events[i]);
         if (ret)
            return ret;
      }
      *event = starpu_event_group_create(num_tasks, events);
      starpu_event_release_all(num_tasks, events);
   }
   else {
      for (i=0; i<num_tasks; i++) {
         ret = starpu_task_submit_ex(tasks[i], num_events, pevents, NULL);
         if (ret)
            return ret;
      }
   }

   return 0;

}

void starpu_display_codelet_stats(struct starpu_codelet_t *cl)
{
	unsigned worker;
	unsigned nworkers = starpu_worker_get_count();

	if (cl->model && cl->model->symbol)
		fprintf(stderr, "Statistics for codelet %s\n", cl->model->symbol);

	unsigned long total = 0;
	
	for (worker = 0; worker < nworkers; worker++)
		total += cl->per_worker_stats[worker];

	for (worker = 0; worker < nworkers; worker++)
	{
		char name[32];
		starpu_worker_get_name(worker, name, 32);

		fprintf(stderr, "\t%s -> %ld / %ld (%2.2f \%%)\n", name, cl->per_worker_stats[worker], total, (100.0f*cl->per_worker_stats[worker])/total);
	}
}

/*
 * We wait for all the tasks that have already been submitted. Note that a
 * regenerable is not considered finished until it was explicitely set as
 * non-regenerale anymore (eg. from a callback).
 */
int starpu_task_wait_for_all(void)
{
	if (STARPU_UNLIKELY(!_starpu_worker_may_perform_blocking_calls()))
		return -EDEADLK;

	PTHREAD_MUTEX_LOCK(&submitted_mutex);

	while (nsubmitted > 0)
		PTHREAD_COND_WAIT(&submitted_cond, &submitted_mutex);
	
	PTHREAD_MUTEX_UNLOCK(&submitted_mutex);

	return 0;
}

void _starpu_decrement_nsubmitted_tasks(void)
{
	PTHREAD_MUTEX_LOCK(&submitted_mutex);

	if (--nsubmitted == 0)
		PTHREAD_COND_BROADCAST(&submitted_cond);

	STARPU_TRACE_UPDATE_TASK_CNT(nsubmitted);

	PTHREAD_MUTEX_UNLOCK(&submitted_mutex);

}

static void _starpu_increment_nsubmitted_tasks(void)
{
	PTHREAD_MUTEX_LOCK(&submitted_mutex);

	nsubmitted++;

	STARPU_TRACE_UPDATE_TASK_CNT(nsubmitted);

	PTHREAD_MUTEX_UNLOCK(&submitted_mutex);
}

void _starpu_initialize_current_task_key(void)
{
	pthread_key_create(&current_task_key, NULL);
}

/* Return the task currently executed by the worker, or NULL if this is called
 * either from a thread that is not a task or simply because there is no task
 * being executed at the moment. */
struct starpu_task *starpu_get_current_task(void)
{
	return pthread_getspecific(current_task_key);
}

starpu_event starpu_get_current_task_event(void)
{
   struct starpu_task * task = starpu_get_current_task();
   if (task == NULL)
      return NULL;
   
	starpu_job_t j = _starpu_get_job_associated_to_task(task);
   starpu_event_retain(j->event);

   return j->event;
}

void _starpu_set_current_task(struct starpu_task *task)
{
	pthread_setspecific(current_task_key, task);
}


