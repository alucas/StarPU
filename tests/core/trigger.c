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

#include <starpu.h>
#include <unistd.h>

void callback(void*data);

volatile int result = 0;

int main() {

   starpu_event event,event2;

	starpu_init(NULL);

   event = starpu_event_create();

   starpu_trigger trigger;
   trigger = _starpu_trigger_create(&callback, NULL, &event2);

   _starpu_trigger_events_register(trigger, 1, &event);

   starpu_trigger_enable(trigger);

   sleep(1);
   result = -2;

   fprintf(stderr, "Trigger enabled. Activating event...\n");

   starpu_event_trigger(event);

   fprintf(stderr, "Event enabled\n");

   starpu_event_wait(event2);

	starpu_shutdown();

   return result;
}

void callback(void*data) {
   result = 0;
   fprintf(stderr, "Trigger triggered\n");
}
