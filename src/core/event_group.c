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

#include <core/event.h>
#include <core/trigger.h>

static void trigger_callback(void*);

starpu_event starpu_event_group_create(int num_events, starpu_event *events) {
   starpu_event event = _starpu_event_create();
   starpu_event_retain(event);

   starpu_trigger trigger = _starpu_trigger_create(&trigger_callback, event);

   _starpu_trigger_events_register(trigger, num_events, events);

   _starpu_trigger_enable(trigger);

   return event;
}

static void trigger_callback(void*data) {
   starpu_event event = (starpu_event)data;

   _starpu_event_complete(event);
   _starpu_event_release_private(event);
}
