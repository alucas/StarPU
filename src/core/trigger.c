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

#include <pthread.h>
#include <core/trigger.h>
#include <starpu_event.h>
#include <drivers/cpu/driver_cpu_event.h>

struct starpu_trigger_t {
   void * data;
   void (*callback)(starpu_trigger, void*);
   int dep_count;
   starpu_event event;
};

void _starpu_trigger_init(starpu_trigger trigger, void (*callback)(starpu_trigger, void*, int num_events, starpu_event * events, starpu_event *event), void *data) {

   trigger->data = data;
   trigger->callback = callback;
   trigger->dep_count = num_events;

   if (event != NULL) {
      starpu_ev ev = _starpu_cpu_event_create();
      *event = ev;
      trigger->event = ev;
   }

   int i;
   for (i=0; i<num_events; i++)
      _starpu_event_trigger_register(events[i], trigger);
}

void _starpu_trigger_signal(starpu_trigger trigger) {
   int dep_count =__sync_sub_and_fetch(trigger->dep_count, 1);
   if (dep_count == 0) {
      _starpu_cpu_event_trigger(trigger->event);
      _starpu_event_release_private(trigger->event);
      trigger->callback(trigger, trigger->data);
   }
}
