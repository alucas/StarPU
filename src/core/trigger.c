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
#include <stdlib.h>
#include <assert.h>
#include <core/trigger.h>
#include <core/event.h>

starpu_trigger _starpu_trigger_create(void (*callback)(void*), void*data, starpu_event *event) {
   starpu_trigger trigger;

   trigger = malloc(sizeof(struct starpu_trigger_t));
   _starpu_trigger_init(trigger, callback, data, event);

   trigger->allocated = 1;

   return trigger;
}

void _starpu_trigger_init(starpu_trigger trigger, void (*callback)(void*), void *data, starpu_event *event) {

   trigger->data = data;
   trigger->callback = callback;
   /* The "+1" is used to enable this trigger */
   trigger->dep_count = 1;
   trigger->enabled = 0;
   trigger->allocated = 0;

   if (event != NULL) {
      trigger->event = _starpu_event_create();
      _starpu_event_retain_private(trigger->event);
      *event = trigger->event;
   }
   else {
      trigger->event = NULL;
   }
}

void _starpu_trigger_events_register(starpu_trigger trigger, int num_events, starpu_event *events) {
   assert(!trigger->enabled);

   __sync_fetch_and_add(&trigger->dep_count, num_events);

   int i;
   for (i=0; i<num_events; i++)
      _starpu_event_trigger_register(events[i], trigger);
}

void _starpu_trigger_enable(starpu_trigger trigger) {
   assert(!trigger->enabled);

   _starpu_trigger_signal(trigger);
   trigger->enabled = 1;
}

void _starpu_trigger_signal(starpu_trigger trigger) {
   int dep_count =__sync_sub_and_fetch(&trigger->dep_count, 1);

   if (dep_count == 0) {
      void (*callback)(void*) = trigger->callback;
      void *data = trigger->data;
      starpu_event event = trigger->event;

      if (trigger->allocated)
         free(trigger);

      if (callback != NULL)
         callback(data);

      if (event != NULL) {
         _starpu_event_complete(event);
         _starpu_event_release_private(event);
      }
   }
}
