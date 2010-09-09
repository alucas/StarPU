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

#include <core/trigger.h>
#include <core/event.h>
#include <core/errorcheck.h>
#include <stdlib.h>

struct event_callback {
   void (*func)(void*);
   void *data;
   struct starpu_trigger_t trigger;
};

void event_callback_callback(void*arg);

int starpu_event_callback_add(starpu_event event, void (*func)(void*), void*data, starpu_event *ev) {
   return starpu_event_group_callback_add(1, &event, func, data, ev);
}

int starpu_event_group_callback_add(int num_events, starpu_event *events, void (*func)(void*), void*data, starpu_event *event) {
   struct event_callback *ec;
   ec = malloc(sizeof(struct event_callback));

   ec->func = func;
   ec->data = data;
   _starpu_trigger_init(&ec->trigger, &event_callback_callback, ec, event);
   _starpu_trigger_events_register(&ec->trigger, num_events, events);
   _starpu_trigger_enable(&ec->trigger);

   return 0;
}

void event_callback_callback(void*arg) {
   struct event_callback *ec = (struct event_callback*)arg;
   void *data = ec->data;
   void (*func)(void*) = ec->func;

   free(arg);

   starpu_worker_status s = _starpu_get_local_worker_status();

   /* We set the flag indicating that this thread can't use blocking methods */
   _starpu_set_local_worker_status(STATUS_CALLBACK);

   func(data);

   /* We can unset the flag */
   _starpu_set_local_worker_status(s);
}
