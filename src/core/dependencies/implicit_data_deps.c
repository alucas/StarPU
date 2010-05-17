/*
 * StarPU
 * Copyright (C) INRIA 2008-2010 (see AUTHORS file)
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
#include <datawizard/datawizard.h>

/* This function adds the implicit task dependencies introduced by data
 * sequential consistency. Two tasks are provided: pre_sync and post_sync which
 * respectively indicates which task is going to depend on the previous deps
 * and on which task future deps should wait. In the case of a dependency
 * introduced by a task submission, both tasks are just the submitted task, but
 * in the case of user interactions with the DSM, these may be different tasks.
 * */
/* NB : handle->sequential_consistency_mutex must be hold by the caller */
void _starpu_detect_implicit_data_deps_with_handle(struct starpu_task *pre_sync_task, struct starpu_task *post_sync_task,
						starpu_data_handle handle, starpu_access_mode mode)
{
	STARPU_ASSERT(!(mode & STARPU_SCRATCH));

	if (handle->sequential_consistency)
	{
		starpu_access_mode previous_mode = handle->last_submitted_mode;
	
		if (mode & STARPU_W)
		{
			if (previous_mode & STARPU_W)
			{
				/* (Read) Write */
				/* This task depends on the previous writer */
				if (handle->last_submitted_writer)
				{
					struct starpu_task *task_array[1] = {handle->last_submitted_writer};
					starpu_task_declare_deps_array(pre_sync_task, 1, task_array);
				}
	
				handle->last_submitted_writer = post_sync_task;
			}
			else {
				/* The task submitted previously were in read-only
				 * mode: this task must depend on all those read-only
				 * tasks and we get rid of the list of readers */
			
				/* Count the readers */
				unsigned nreaders = 0;
				struct starpu_task_list *l;
				l = handle->last_submitted_readers;
				while (l)
				{
					nreaders++;
					l = l->next;
				}
	
				struct starpu_task *task_array[nreaders];
				unsigned i = 0;
				l = handle->last_submitted_readers;
				while (l)
				{
					STARPU_ASSERT(l->task);
					task_array[i++] = l->task;

					struct starpu_task_list *prev = l;
					l = l->next;
					free(prev);
				}
	
				handle->last_submitted_readers = NULL;
				handle->last_submitted_writer = post_sync_task;
	
				starpu_task_declare_deps_array(pre_sync_task, nreaders, task_array);
			}
	
		}
		else {
			/* Add a reader */
			STARPU_ASSERT(pre_sync_task);
			STARPU_ASSERT(post_sync_task);
	
			/* Add this task to the list of readers */
			struct starpu_task_list *link = malloc(sizeof(struct starpu_task_list));
			link->task = post_sync_task;
			link->next = handle->last_submitted_readers;
			handle->last_submitted_readers = link;


			/* This task depends on the previous writer if any */
			if (handle->last_submitted_writer)
			{
				struct starpu_task *task_array[1] = {handle->last_submitted_writer};
				starpu_task_declare_deps_array(pre_sync_task, 1, task_array);
			}
		}
	
		handle->last_submitted_mode = mode;
	}
}

/* Create the implicit dependencies for a newly submitted task */
void _starpu_detect_implicit_data_deps(struct starpu_task *task)
{
	STARPU_ASSERT(task->cl);

	unsigned nbuffers = task->cl->nbuffers;

	unsigned buffer;
	for (buffer = 0; buffer < nbuffers; buffer++)
	{
		starpu_data_handle handle = task->buffers[buffer].handle;
		starpu_access_mode mode = task->buffers[buffer].mode;

		/* Scratch memory does not introduce any deps */
		if (mode & STARPU_SCRATCH)
			continue;

		PTHREAD_MUTEX_LOCK(&handle->sequential_consistency_mutex);
		_starpu_detect_implicit_data_deps_with_handle(task, task, handle, mode);
		PTHREAD_MUTEX_UNLOCK(&handle->sequential_consistency_mutex);
	}
}

/* This function is called when a task has been executed so that we don't
 * create dependencies to task that do not exist anymore. */
void _starpu_release_data_enforce_sequential_consistency(struct starpu_task *task, starpu_data_handle handle)
{
	PTHREAD_MUTEX_LOCK(&handle->sequential_consistency_mutex);

	if (handle->sequential_consistency)
	{
		/* If this is the last writer, there is no point in adding
		 * extra deps to that tasks that does not exists anymore */
		if (task == handle->last_submitted_writer)
			handle->last_submitted_writer = NULL;

		/* Same if this is one of the readers: we go through the list
		 * of readers and remove the task if it is found. */
		struct starpu_task_list *l;
		l = handle->last_submitted_readers;
		struct starpu_task_list *prev = NULL;
		while (l)
		{
			struct starpu_task_list *next = l->next;

			if (l->task == task)
			{
				/* If we found the task in the reader list */
				free(l);

				if (prev)
				{
					prev->next = next;
				}
				else {
					/* This is the first element of the list */
					handle->last_submitted_readers = next;
				}
			}
			else {
				prev = l;
			}

			l = next;
		}
	}

	PTHREAD_MUTEX_UNLOCK(&handle->sequential_consistency_mutex);
}

void _starpu_add_post_sync_tasks(struct starpu_task *post_sync_task, starpu_data_handle handle)
{
	PTHREAD_MUTEX_LOCK(&handle->sequential_consistency_mutex);

	if (handle->sequential_consistency)
	{
		handle->post_sync_tasks_cnt++;

		struct starpu_task_list *link = malloc(sizeof(struct starpu_task_list));
		link->task = post_sync_task;
		link->next = handle->post_sync_tasks;
		handle->post_sync_tasks = link;		
	}

	PTHREAD_MUTEX_UNLOCK(&handle->sequential_consistency_mutex);
}

void _starpu_unlock_post_sync_tasks(starpu_data_handle handle)
{
	struct starpu_task_list *post_sync_tasks = NULL;
	unsigned do_submit_tasks = 0;

	PTHREAD_MUTEX_LOCK(&handle->sequential_consistency_mutex);

	if (handle->sequential_consistency)
	{
		STARPU_ASSERT(handle->post_sync_tasks_cnt > 0);

		if (--handle->post_sync_tasks_cnt == 0)
		{
			/* unlock all tasks : we need not hold the lock while unlocking all these tasks */
			do_submit_tasks = 1;
			post_sync_tasks = handle->post_sync_tasks;
			handle->post_sync_tasks = NULL;
		}

	}

	PTHREAD_MUTEX_UNLOCK(&handle->sequential_consistency_mutex);

	if (do_submit_tasks)
	{
		struct starpu_task_list *link = post_sync_tasks;

		while (link) {
			/* There is no need to depend on that task now, since it was already unlocked */
			_starpu_release_data_enforce_sequential_consistency(link->task, handle);

			int ret = starpu_task_submit(link->task);
			STARPU_ASSERT(!ret);
			link = link->next;
		}
	}
}
