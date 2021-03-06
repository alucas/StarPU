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

#ifndef __STARPU_TASK_H__
#define __STARPU_TASK_H__

#include <errno.h>
#include <starpu_config.h>
#include <starpu_event.h>
#include <starpu.h>

#ifdef STARPU_USE_CUDA
#include <cuda.h>
#endif

#include <starpu_data.h>

#define STARPU_CPU	((1ULL)<<1)
#define STARPU_CUDA	((1ULL)<<3)
#define STARPU_SPU	((1ULL)<<4)
#define STARPU_GORDON	((1ULL)<<5)
#define STARPU_OPENCL	((1ULL)<<6)

/* task status */
#define STARPU_TASK_INVALID	0
#define STARPU_TASK_BLOCKED	1
#define STARPU_TASK_READY	2
#define STARPU_TASK_RUNNING	3
#define STARPU_TASK_FINISHED	4

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A codelet describes the various function 
 * that may be called from a worker
 */
typedef struct starpu_codelet_t {
	/* where can it be performed ? */
	uint32_t where;

	/* the different implementations of the codelet */
	void (*cuda_func)(void **, void *);
	void (*cpu_func)(void **, void *);
	void (*opencl_func)(void **, void *);
	uint8_t gordon_func;

	/* how many buffers do the codelet takes as argument ? */
	unsigned nbuffers;

	struct starpu_perfmodel_t *model;

	/* statistics collected at runtime: this is filled by StarPU and should
	 * not be accessed directly (use the starpu_display_codelet_stats
	 * function instead for instance). */
	unsigned long per_worker_stats[STARPU_NMAXWORKERS];
} starpu_codelet;

struct starpu_task {
	struct starpu_codelet_t *cl;

	/* arguments managed by the DSM */
	struct starpu_buffer_descr_t buffers[STARPU_NMAXBUFS];
	void *interface[STARPU_NMAXBUFS];

	/* arguments not managed by the DSM are given as a buffer */
	void *cl_arg;
	/* in case the argument buffer has to be uploaded explicitely */
	size_t cl_arg_size;
	
	/* when the task is done, callback_func(callback_arg) is called */
	void (*callback_func)(void *);
	void *callback_arg;

	/* options for the task execution */
	unsigned synchronous; /* if set, a call to push is blocking */
	int priority; /* STARPU_MAX_PRIO = most important 
        		: STARPU_MIN_PRIO = least important */

	/* in case the task has to be executed on a specific worker */
	unsigned execute_on_a_specific_worker;
	unsigned workerid;

	/* If that flag is set, the task structure will automatically be freed,
	 * either after the execution of the callback. If this flag is not set,
	 * dynamically allocated data structures will not be freed until
	 * starpu_task_destroy is called explicitely. Setting this flag for a
	 * statically allocated task structure will result in undefined
	 * behaviour. */
	int destroy;

	/* If this flag is set, the task will be re-submitted to StarPU once it
	 * has been executed. This flag must not be set if the destroy flag is
	 * set too. */ 
	int regenerate;

	unsigned status;

	/* Predicted duration of the task. This field is only valid if the
	 * scheduling strategy uses performance models. */
	double predicted;

	/* This field are provided for the convenience of the scheduler. */
	struct starpu_task *prev;
	struct starpu_task *next;

	/* this is private to StarPU, do not modify. If the task is allocated
	 * by hand (without starpu_task_create), this field should be set to
	 * NULL. */
	void *starpu_private;
};

/* It is possible to initialize statically allocated tasks with this value.
 * This is equivalent to initializing a starpu_task structure with the
 * starpu_task_init function. */
#define STARPU_TASK_INITIALIZER 			\
{							\
	.cl = NULL,					\
	.cl_arg = NULL,					\
	.cl_arg_size = 0,				\
	.callback_func = NULL,				\
	.callback_arg = NULL,				\
	.priority = STARPU_DEFAULT_PRIO,                \
	.synchronous = 0,				\
	.execute_on_a_specific_worker = 0,		\
	.destroy = 0,					\
	.regenerate = 0,				\
	.status = STARPU_TASK_INVALID,			\
	.predicted = -1.0,				\
	.starpu_private = NULL				\
};

/* task depends on the tasks in task array */
void starpu_task_declare_deps_array(struct starpu_task *task, unsigned num_events, starpu_event * events);

/* Initialize a task structure with default values. */
void starpu_task_init(struct starpu_task *task);

/* Release all the structures automatically allocated to execute the task. This
 * is called implicitely by starpu_task_destroy, but the task structure itself
 * is not freed. This should be used for statically allocated tasks for
 * instance. */
void starpu_task_deinit(struct starpu_task *task);

/* Allocate a task structure and initialize it with default values. Tasks
 * allocated dynamically with starpu_task_create are automatically freed when
 * the task is terminated. If the destroy flag is explicitely unset, the
 * ressources used by the task are freed by calling starpu_task_destroy.
 * */
struct starpu_task *starpu_task_create(void);

/* Free the ressource allocated during the execution of the task and deallocate
 * the task structure itself. This function can be called automatically after
 * the execution of a task by setting the "destroy" flag of the starpu_task
 * structure (default behaviour). Calling this function on a statically
 * allocated task results in an undefined behaviour. */
void starpu_task_destroy(struct starpu_task *task);

int starpu_task_submit(struct starpu_task *task, starpu_event *event);
int starpu_task_submit_all(int num_tasks, struct starpu_task **tasks, starpu_event *event);

/* Add dependencies to "num_events" events in "events" array, then submit the task */
int starpu_task_submit_ex(struct starpu_task *task, int num_events, starpu_event *events, starpu_event *event);
int starpu_task_submit_all_ex(int num_tasks, struct starpu_task **tasks, int num_events, starpu_event *pevents, starpu_event *event);

/* This function waits until all the tasks that were already submitted have
 * been executed. */
int starpu_task_wait_for_all(void);

void starpu_display_codelet_stats(struct starpu_codelet_t *cl);

/* Return the task currently executed by the worker, or NULL if this is called
 * either from a thread that is not a task or simply because there is no task
 * being executed at the moment. */
struct starpu_task *starpu_get_current_task(void);

/* Return the event taht will be triggered when the current task terminates
 * or NULL if this is called either from a thread that is not a task or simply
 * because there is no task being executed at the moment.
 * Obtained event has to be released with starpu_event_release.
 */
starpu_event starpu_get_current_task_event(void);

#ifdef __cplusplus
}
#endif

#endif // __STARPU_TASK_H__
