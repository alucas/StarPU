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

int _starpu_event_create(starpu_event * event, starpu_event_methods methods, void *data) {
   int err;
   starpu_event ev;

   assert(event != NULL);

   ev = malloc(sizeof(struct starpu_event_t));
   *event = ev;

   ev->ref_count = 0;
   ev->ref_count_priv = 1;
   ev->data = data;
   ev->methods = methods;

   err = pthread_mutex_init(&ev->mutex, NULL);
   if (err != 0)
      return err;

   err = ev->methods->init(ev);

   return err;
}

int _starpu_event_retain_private(starpu_event event) {
   int ret;

   pthread_mutex_lock(&event->mutex);
   
   event->ref_count_priv++;

   ret = event->ref_count_priv;

   pthread_mutex_unlock(&event->mutex);

   return ret;
}

int _starpu_event_release_private(starpu_event event) {
   int ret;

   pthread_mutex_lock(&event->mutex);

   event->ref_count_priv--;
   ret = event->ref_count_priv;

   pthread_mutex_unlock(&event->mutex);

   if (ret == 0 && event->ref_count == 0)
      _starpu_event_free(event);

   return ret;
}

/* This method is called when both ref_count and ref_count_priv
 * are equal to 0.
 */
int _starpu_event_free(starpu_event event) {
   event->methods->free(event);

   pthread_mutex_destroy(&event->mutex);
}
