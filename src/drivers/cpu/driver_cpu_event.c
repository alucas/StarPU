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

#ifdef STARPU_USE_CPU

#include <pthread.h>
#include <starpu_event.h>
#include <core/event.h>
#include "driver_cpu_event.h"

static int _starpu_cpu_event_init(starpu_event);
static int _starpu_cpu_event_wait(starpu_event);
static int _starpu_cpu_event_free(starpu_event);
static starpu_event_status_t _starpu_cpu_event_status(starpu_event);

static struct starpu_event_methods_t methods = {
   .wait = _starpu_cpu_event_wait,
   .free = _starpu_cpu_event_free,
   .status = _starpu_cpu_event_status
};

typedef struct cpu_event_t {
   starpu_event_status_t status;
   pthread_cond_t cond;
   pthread_mutex_t mutex;
} * cpu_event;


starpu_event _starpu_cpu_event_create() {
   cpu_event event;
   event = malloc(sizeof(struct cpu_event_t));

   pthread_cond_init(&event->cond);
   pthread_mutex_init(&event->mutex);

   event->status = STARPU_EVENT_WAITING;

   return _starpu_event_create(&methods, event);
}

int _starpu_cpu_event_trigger(starpu_event event) {
   cpu_event ev = (cpu_event)_starpu_event_data(event);

   /* New dependencies will directly use status instead of the condition */
   pthread_mutex_lock(event->mutex);
   event->status = STARPU_EVENT_COMPLETE;
   pthread_mutex_unlock(event->mutex);
   
   /* Now we can wake-up dependencies waiting on the cond */
   pthread_cond_broadcast(&event->cond);
}

static int _starpu_cpu_event_wait(starpu_event event) {
   cpu_event ev = (cpu_event)_starpu_event_data(event);
   pthread_mutex_lock(ev->mutex);
   pthread_cond_wait(ev->cond, ev->mutex);
   pthread_mutex_unlock(ev->mutex);

   return 0;
}

static int _starpu_cpu_event_free(starpu_event event) {
   cpu_event ev = (cpu_event)_starpu_event_data(event);

   pthread_mutex_destroy(ev->mutex);
   pthread_cond_destroy(ev->cond);
   free(ev);

   return 0;
}

static starpu_event_status_t _starpu_cpu_event_status(starpu_event event) {
   cpu_event ev = (cpu_event)_starpu_event_data(event);
   return ev->status;
}

#endif // STARPU_USE_CPU

