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

int starpu_event_release(starpu_event event);
int starpu_event_retain(starpu_event event);

int starpu_events_wait(int num_events, starpu_event *events);


typedef enum {
   STARPU_EVENT_EXECUTION_STATUS
} starpu_event_info;

int starpu_event_info_get(starpu_event event, starpu_event_info info, void *ret);


#ifdef __cplusplus
}
#endif

#endif // __STARPU_EVENT_H__
