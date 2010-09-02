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

#ifndef __EVENT_H__
#define __EVENT_H__

#include <pthread.h>
#include <starpu_event.h>
#include <core/trigger.h>

typedef struct starpu_event_methods_t {
   /* Wait for event completion */
   int (*wait)(starpu_event);

   /* Free */
   int (*free)(starpu_event);
} * starpu_event_methods;


/* Create an event */
starpu_event _starpu_event_create();

/* Lock an event */
int _starpu_event_lock(starpu_event);

/* Try to lock an event */
int _starpu_event_trylock(starpu_event);

/* Unock an event */
int _starpu_event_unlock(starpu_event);

/* Increment event private reference counter.
 * This counter avoids user premature event release
 */
int _starpu_event_retain_private(starpu_event);

/* Decrement event private reference counter */
int _starpu_event_release_private(starpu_event);

/* Trigger registering */
int _starpu_event_trigger_register(starpu_event, starpu_trigger);

/* Signal that the event occured */
void _starpu_event_complete(starpu_event);

/* Profiling */
int _starpu_event_profiling_submit_time_set(starpu_event, struct timespec*);
int _starpu_event_profiling_start_time_set(starpu_event, struct timespec*);
int _starpu_event_profiling_end_time_set(starpu_event, struct timespec*);
int _starpu_event_profiling_worker_id_set(starpu_event, int wid);

#endif // __EVENT_H__
