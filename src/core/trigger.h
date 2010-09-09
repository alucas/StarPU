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

#ifndef __TRIGGER_H__
#define __TRIGGER_H__

#include <starpu_event.h>

typedef struct starpu_trigger_t * starpu_trigger;

struct starpu_trigger_t {
   void * data;
   void (*callback)(void*);
   int dep_count;

   int enabled;
   int allocated;
   starpu_event event;
};

starpu_trigger _starpu_trigger_create(void (*callback)(void*), void*data, starpu_event *event);

void _starpu_trigger_init(starpu_trigger trigger, void (*callback)(void*), void *data, starpu_event *event);

void _starpu_trigger_events_register(starpu_trigger, int num_events, starpu_event *events);

void _starpu_trigger_enable(starpu_trigger trigger);

void _starpu_trigger_signal(starpu_trigger trigger);

#endif // __TRIGGER_H__
