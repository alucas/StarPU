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

#include <starpu_util.h>
#include <starpu_profiling.h>
#include <pthread.h>
#include <stdlib.h>
#include <assert.h>
#include <core/event.h>
#include <core/errorcheck.h>
#include <common/timing.h>
#include <profiling/profiling.h>

struct starpu_event_t {
   /* Indicates if it is a user event */
   int user_event;

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

   /* Profiling */
   int profiling_enabled;
	struct timespec prof_submit_time;
	struct timespec prof_start_time;
	struct timespec prof_end_time;
	int prof_workerid;

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

int starpu_event_release_all(int num_events, starpu_event *events) {
   int i;
   for (i=0; i<num_events; i++)
      starpu_event_release(events[i]);

   return 0;
}

int starpu_event_retain(starpu_event event) {
   _starpu_event_lock(event);
   
   event->ref_count++;

   _starpu_event_unlock(event);

   return 0;
}

int starpu_event_retain_all(int num_events, starpu_event *events) {
   int i;
   for (i=0; i<num_events; i++)
      starpu_event_retain(events[i]);

   return 0;
}

int starpu_event_wait(starpu_event event) {
	if (STARPU_UNLIKELY(!_starpu_worker_may_perform_blocking_calls()))
		return -EDEADLK;

   /* We can avoid mutex locking if event is already complete */
   if (!event->complete) {

      _starpu_event_lock(event);
      
      event->cond_wait_count += 1;

      while (!event->complete) {
         pthread_cond_wait(&event->cond, &event->mutex);
      }

      event->cond_wait_count -= 1;

      _starpu_event_unlock(event);
   }

   return 0;
}

int starpu_event_wait_all(int num_events, starpu_event *events) {
	if (STARPU_UNLIKELY(!_starpu_worker_may_perform_blocking_calls()))
		return -EDEADLK;

   int i;
   for (i=0; i<num_events; i++)
      starpu_event_wait(events[i]);

   return 0;
}

int starpu_event_wait_and_release(starpu_event event) {
   starpu_event_wait(event);
   starpu_event_release(event);

   return 0;
}

int starpu_event_wait_and_release_all(int num_events, starpu_event *events) {
   int i;
   for (i=0; i<num_events; i++)
      starpu_event_wait_and_release(events[i]);

   return 0;
}

int starpu_event_test(starpu_event event) {
   return event->complete;
}

int starpu_event_test_all(int num_events, starpu_event *events) {
   int i;
   for(i=0; i<num_events; i++) {
      if (!starpu_event_test(events[i]))
         return 0;
   }

   return 1;
}

starpu_event starpu_event_create() {
   starpu_event event = _starpu_event_create();
   event->user_event = 1;
   event->ref_count = 1;
   event->ref_count_priv = 0;
   return event;
}

void starpu_event_trigger(starpu_event event) {
   assert(event->user_event);
   _starpu_event_complete(event);
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

   ev->user_event = 0;

   ev->ref_count = 0;
   ev->ref_count_priv = 1;
   ev->complete = 0;

   ev->trigger_count = 0;
   ev->trigger_size = 5;
   ev->triggers = malloc(ev->trigger_size * sizeof(starpu_trigger));

   pthread_mutex_init(&ev->mutex, NULL);
   pthread_cond_init(&ev->cond, NULL);
   ev->cond_wait_count = 0;

   /* Profiling */
   if (!starpu_profiling_enabled()) {
      ev->profiling_enabled = 0;
   }
   else {
      ev->profiling_enabled = 1;
		starpu_clock_gettime(&ev->prof_submit_time);
		starpu_timespec_clear(&ev->prof_start_time);
		starpu_timespec_clear(&ev->prof_end_time);
      ev->prof_workerid = -1;
   }

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
   int signal = 0;

   /* If the event is complete, we can avoid a mutex lock/unlock */
   if (event->complete) {
      signal = 1;
   }
   else {

      _starpu_event_retain_private(event);

      _starpu_event_lock(event);

      if (event->complete) {
         /* we don't need to register because the event is complete */
         signal = 1;
      }
      else {
         /* Register trigger */
         if (event->trigger_count == event->trigger_size) {
            event->trigger_size *= 2;
            event->triggers = realloc(event->triggers, event->trigger_size * sizeof(starpu_trigger));
         }
         event->triggers[event->trigger_count] = trigger;
         event->trigger_count += 1;
      }

      _starpu_event_unlock(event);

   }


   if (signal)
      _starpu_trigger_signal(trigger);

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
      _starpu_event_release_private(event);
   }

   _starpu_event_release_private(event);
}

/* This method is called when both ref_count and ref_count_priv
 * are equal to 0.
 */
int _starpu_event_free(starpu_event event) {
   assert(event->ref_count == 0 && event->ref_count_priv == 0);

   while (event->cond_wait_count != 0);

   pthread_mutex_destroy(&event->mutex);
   pthread_cond_destroy(&event->cond);

   free(event->triggers);

   return 0;
}

int starpu_event_profiling_enabled(starpu_event event) {
   return event->profiling_enabled;
}

int _starpu_event_profiling_submit_time_set(starpu_event event, struct timespec* ts) {
   memcpy(&event->prof_submit_time, ts, sizeof(struct timespec));
   return 0;
}

int _starpu_event_profiling_start_time_set(starpu_event event, struct timespec* ts) {
   memcpy(&event->prof_start_time, ts, sizeof(struct timespec));
   return 0;
}

int _starpu_event_profiling_end_time_set(starpu_event event, struct timespec* ts) {
   memcpy(&event->prof_end_time, ts, sizeof(struct timespec));
   return 0;
}

int _starpu_event_profiling_worker_id_set(starpu_event event, int wid) {
   event->prof_workerid = wid;
   return 0;
}


void starpu_event_profiling_submit_time(starpu_event event, struct timespec* ts) {
   memcpy(ts, &event->prof_submit_time, sizeof(struct timespec));
}

void starpu_event_profiling_start_time(starpu_event event, struct timespec* ts) {
   memcpy(ts, &event->prof_start_time, sizeof(struct timespec));
}

void starpu_event_profiling_end_time(starpu_event event, struct timespec* ts) {
   memcpy(ts, &event->prof_end_time, sizeof(struct timespec));
}

int starpu_event_profiling_worker_id(starpu_event event) {
   return event->prof_workerid;
}
