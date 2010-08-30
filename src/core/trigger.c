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
#include <assert.h>
#include <core/trigger.h>
#include <starpu_event.h>
#include <core/event.h>

struct starpu_trigger_t {
   void * data;
   void (*callback)(starpu_trigger, void*);
   int dep_count;
   starpu_event event;

   int enabled;
};

void _starpu_trigger_init(starpu_trigger trigger, int num_events, starpu_event * events, void (*callback)(starpu_trigger, void*), void *data, starpu_event *event) {

   trigger->data = data;
   trigger->callback = callback;
   trigger->dep_count = num_events+1;
   trigger->enabled = 0;

   if (event != NULL) {
      starpu_event ev = _starpu_event_create();
      *event = ev;
      trigger->event = ev;
   }

   _starpu_trigger_events_register(trigger, num_events, events);
}

void _starpu_trigger_events_register(starpu_trigger trigger, int num_events, starpu_event *events) {
   assert(!trigger->enabled);

   int i;
   for (i=0; i<num_events; i++)
      _starpu_event_trigger_register(events[i], trigger);
}

void _starpu_trigger_enable(starpu_trigger trigger) {
   assert(!trigger->enabled);
   _starpu_trigger_signal(trigger);
}

void _starpu_trigger_signal(starpu_trigger trigger) {
   int dep_count =__sync_sub_and_fetch(&trigger->dep_count, 1);
   if (dep_count == 0) {
      if (trigger->event != NULL)
         _starpu_event_complete(trigger->event);
      _starpu_event_release_private(trigger->event);
      trigger->callback(trigger, trigger->data);
   }
}
