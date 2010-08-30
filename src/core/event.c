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
#include <core/event.h>
#include <core/errorcheck.h>
#include <starpu_util.h>

struct starpu_event_t {
   /* Public reference counter */
   volatile int ref_count;

   /* Private reference counter */
   volatile int ref_count_priv;

   /* Indicates if this event is complete */
   volatile int complete;


   /* Mutex & Cond */
   pthread_mutex_t mutex;
   pthread_cond_t cond;
   volatile int cond_wait_count;

   /* Associated triggers */
   int trigger_count;
   int trigger_size;
   starpu_trigger * triggers;
};


int _starpu_event_free(starpu_event);


/******************/
/* IMPLEMENTATION */
/******************/

/* PUBLIC */

int starpu_event_release(starpu_event event) {
   _starpu_event_lock(event);

   event->ref_count--;

   int free = (event->ref_count == 0 && event->ref_count_priv == 0);

   _starpu_event_unlock(event);

   if (free)
      _starpu_event_free(event);

   return 0;
}

int starpu_event_retain(starpu_event event) {
   _starpu_event_lock(event);
   
   event->ref_count++;

   _starpu_event_unlock(event);

   return 0;
}

int starpu_event_wait(starpu_event event) {
	if (STARPU_UNLIKELY(!_starpu_worker_may_perform_blocking_calls()))
		return -EDEADLK;

   _starpu_event_lock(event);
   
   event->cond_wait_count += 1;

   while (!event->complete) {
      pthread_cond_wait(&event->cond, &event->mutex);
   }

   event->cond_wait_count -= 1;

   _starpu_event_unlock(event);

   return 0;
}

int starpu_event_wait_all(int num_events, starpu_event *events) {
   int i;
   for (i=0; i<num_events; i++)
      starpu_event_wait(events[i]);

   return 0;
}

int starpu_event_test(starpu_event event) {
   return event->complete;
}

/* PRIVATE */

int _starpu_event_lock(starpu_event event) {
   return pthread_mutex_lock(&event->mutex);
}

int _starpu_event_unlock(starpu_event event) {
   return pthread_mutex_unlock(&event->mutex);
}

int _starpu_event_trylock(starpu_event event) {
   return pthread_mutex_trylock(&event->mutex);
}

starpu_event _starpu_event_create() {
   starpu_event ev;

   ev = malloc(sizeof(struct starpu_event_t));

   ev->ref_count = 0;
   ev->ref_count_priv = 1;
   ev->complete = 0;

   ev->trigger_count = 0;
   ev->trigger_size = 5;
   ev->triggers = malloc(ev->trigger_size * sizeof(starpu_trigger));

   pthread_mutex_init(&ev->mutex, NULL);
   pthread_cond_init(&ev->cond, NULL);
   ev->cond_wait_count = 0;

   return ev;
}

int _starpu_event_retain_private(starpu_event event) {
   _starpu_event_lock(event);
   
   event->ref_count_priv++;

   _starpu_event_unlock(event);

   return 0;
}

int _starpu_event_release_private(starpu_event event) {
   _starpu_event_lock(event);

   event->ref_count_priv--;

   int free = (event->ref_count_priv == 0 && event->ref_count == 0);

   _starpu_event_unlock(event);

   if (free)
      _starpu_event_free(event);

   return 0;
}

/* Trigger registering */
int _starpu_event_trigger_register(starpu_event event, starpu_trigger trigger) {
   _starpu_event_lock(event);

   if (event->complete) {
      /* we don't need to register because the event is complete */
      _starpu_trigger_signal(trigger);
   }
   else {
      /* Register trigger */
      if (event->trigger_count == event->trigger_size) {
         event->triggers = realloc(event->triggers, 2 * event->trigger_size * sizeof(starpu_trigger));
      }
      event->triggers[event->trigger_count] = trigger;
      event->trigger_count += 1;
   }

   _starpu_event_unlock(event);

   return 0;
}

void _starpu_event_complete(starpu_event event) {
   _starpu_event_lock(event);

   assert(!event->complete);
   event->complete = 1;

   pthread_cond_broadcast(&event->cond);

   _starpu_event_unlock(event);

   int i;
   for (i=0; i<event->trigger_count; i++) {
      _starpu_trigger_signal(event->triggers[i]);
   }

   _starpu_event_release_private(event);
}

/* This method is called when both ref_count and ref_count_priv
 * are equal to 0.
 */
int _starpu_event_free(starpu_event event) {
   assert(event->complete && event->ref_count == 0 && event->ref_count_priv == 0);

   while (event->cond_wait_count != 0);

   pthread_mutex_destroy(&event->mutex);
   pthread_cond_destroy(&event->cond);

   return 0;
}
