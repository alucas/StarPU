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

void callback(void*);

int main() {

   starpu_event event;
   int val = 0;
   unsigned count = 10;

	starpu_init(NULL);

   event = starpu_event_create();

   starpu_event_callback_add(event, &callback, &val, NULL);

   starpu_event_trigger(event);
   starpu_event_release(event);

   while ((volatile int)val != 999 && count != 0) {
      count -= 1;
      usleep(50);
   }

	starpu_shutdown();

   return (val != 999 ? -1 : 0);
}

void callback(void*data) {
   int * val = (int*)data;
   *val = 999;
}
