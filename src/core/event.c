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

struct starpu_event_t {
   /* Public reference counter */
   int ref_count;

   /* Private reference counter */
   int ref_count_priv;

   /* Implementation specific data */
   void * data;

   /* Associated mutex */
   pthread_mutex_t mutex;

   /* Methods */
   starpu_event_methods methods;
};

int _starpu_event_free(starpu_event);


/******************/
/* IMPLEMENTATION */
/******************/

/* PUBLIC */

int starpu_event_release(starpu_event event) {
   int ret;

   _starpu_event_lock(event);

   event->ref_count--;
   ret = event->ref_count;

   if (ret != 0 || event->ref_count_priv != 0)
      _starpu_event_unlock(event);
   else
      _starpu_event_free(event);

   return ret;
}

int starpu_event_retain(starpu_event event) {
   int ret;

   _starpu_event_lock(event);
   
   event->ref_count++;
   ret = event->ref_count;

   _starpu_event_unlock(event);

   return ret;
}

int starpu_event_wait(starpu_event event) {
   if (starpu_event_status(event) == STARPU_EVENT_COMPLETE)
      return 0;
   else
      return event->methods->wait(event);
}

int starpu_event_wait_all(int num_events, starpu_event *events) {
   int i;
   for (i=0; i<num_events; i++) {
      int err;

      err = starpu_event_wait(events[i]);
      if (err != 0)
         return err;
   }

   return 0;
}

starpu_event_status_t starpu_event_status(starpu_event event) {
   return event->methods->status(event);
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

starpu_event _starpu_event_create(starpu_event_methods methods, void *data) {
   int err;
   starpu_event ev;

   ev = malloc(sizeof(struct starpu_event_t));

   ev->ref_count = 0;
   ev->ref_count_priv = 1;
   ev->data = data;
   ev->methods = methods;

   err = pthread_mutex_init(&ev->mutex, NULL);
   if (err != 0)
      return NULL;

   err = ev->methods->init(ev);
   if (err != 0)
      return NULL;

   return ev;
}

int _starpu_event_retain_private(starpu_event event) {
   int ret;

   _starpu_event_lock(event);
   
   event->ref_count_priv++;
   ret = event->ref_count_priv;

   _starpu_event_unlock(event);

   return ret;
}

int _starpu_event_release_private(starpu_event event) {
   int ret;

   _starpu_event_lock(event);

   event->ref_count_priv--;
   ret = event->ref_count_priv;

   if (ret != 0 || event->ref_count != 0)
      _starpu_event_unlock(event);
   else
      _starpu_event_free(event);

   return ret;
}

/* Get event data */
void * _starpu_event_data(starpu_event event) {
   return event->data;
}

/* This method is called when both ref_count and ref_count_priv
 * are equal to 0.
 */
int _starpu_event_free(starpu_event event) {
   event->methods->free(event);

   pthread_mutex_destroy(&event->mutex);

   return 0;
}
