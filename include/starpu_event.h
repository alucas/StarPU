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

#ifndef __STARPU_EVENT_H__
#define __STARPU_EVENT_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct starpu_event_t * starpu_event;

int starpu_event_release(starpu_event);
int starpu_event_retain(starpu_event);

int starpu_event_wait(starpu_event);
int starpu_event_wait_all(int num_events, starpu_event *events);
int starpu_event_test(starpu_event);

/* User events */
starpu_event starpu_event_create();
void starpu_event_trigger(starpu_event);

/* Profiling
 * These functions require that StarPU profiling is enabled
 */
void starpu_event_profiling_submit_time(starpu_event, struct timespec*);
void starpu_event_profiling_start_time(starpu_event, struct timespec*);
void starpu_event_profiling_end_time(starpu_event, struct timespec*);
int starpu_event_profiling_worker_id(starpu_event);

#ifdef __cplusplus
}
#endif

#endif // __STARPU_EVENT_H__
