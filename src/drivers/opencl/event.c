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

#include <CL/cl.h>
#include <starpu_event.h>
#include <starpu_opencl.h>
#include <core/event.h>
#include <common/common.h>
#include <common/list.h>
#include <drivers/opencl/event.h>

/* OpenCL event manager
 * This is only useful for OpenCL1.0 which doesn't support clSetEventCallback
 */
LIST_TYPE(opencl_event_binding,
   starpu_event event;
   cl_event clevent;
);

static opencl_event_binding_list_t blist;
static pthread_mutex_t blist_mutex;
static pthread_cond_t blist_cond;
static pthread_t thread;

void * thread_routine(void *UNUSED(arg)) {
   while (1) {
      pthread_mutex_lock(&blist_mutex);
      
      if (opencl_event_binding_list_empty(blist))
         pthread_cond_wait(&blist_cond, &blist_mutex);

      pthread_mutex_unlock(&blist_mutex);

      /* Walkthrough bindings and test for completion  */
      opencl_event_binding_t b  = opencl_event_binding_list_begin(blist);
      while (b != opencl_event_binding_list_end(blist)) {

         cl_int status;
         clGetEventInfo(b->clevent, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(cl_int), &status, NULL);

         opencl_event_binding_t b2 = b;
         b = opencl_event_binding_list_next(b);

         if (status == CL_COMPLETE) {
            _starpu_event_complete(b2->event);
            clReleaseEvent(b2->clevent);

            pthread_mutex_lock(&blist_mutex);
            opencl_event_binding_list_erase(blist, b2);
            pthread_mutex_unlock(&blist_mutex);

            opencl_event_binding_delete(b2);
         }

      }
   }

   return NULL;
}

void _starpu_opencl_events_init() {
   pthread_mutex_init(&blist_mutex, NULL);
   pthread_cond_init(&blist_cond, NULL);
   pthread_create(&thread, NULL, thread_routine, NULL);

   blist = opencl_event_binding_list_new();
}

int _starpu_opencl_event_version(cl_event event) {
   cl_context ctx;
   cl_device_id dev;
   char version[1024];
   cl_int err;

   /* FIXME Ugly hack!!!
    * clGetEventInfo doesn't return context........ (nvidia 3.2.1, 260.24)
    */
   return 10;

   err = clGetEventInfo(event, CL_EVENT_CONTEXT, sizeof(cl_context), &ctx, NULL);
   if (err != CL_SUCCESS) STARPU_OPENCL_REPORT_ERROR(err);

   //We assume there is one context per device
   err = clGetContextInfo(ctx, CL_CONTEXT_DEVICES, sizeof(cl_device_id), &dev, NULL);
   if (err != CL_SUCCESS) STARPU_OPENCL_REPORT_ERROR(err);

   err = clGetDeviceInfo(dev, CL_DEVICE_VERSION, 1024, version, NULL);
   if (err != CL_SUCCESS) STARPU_OPENCL_REPORT_ERROR(err);

   switch (version[11]) {
      case '1': 
         return 11; //OpenCL 1.1
      case '0':
         return 10; //OpenCL 1.0
      default:
         return 10;
   }
}

static void opencl_event_callback(cl_event event, cl_int UNUSED(status), void *user_data) {
   starpu_event ev = (starpu_event)user_data;

   _starpu_event_complete(ev);

   clReleaseEvent(event);
}

starpu_event _starpu_opencl_event_create(cl_event event) {
   
   starpu_event ev = _starpu_event_create();

   _starpu_opencl_event_bind(event, ev);

   return ev;
}

int _starpu_opencl_event_bind(cl_event clevent, starpu_event event) {

   _starpu_event_retain_private(event);
   clRetainEvent(clevent);

   if (_starpu_opencl_event_version(clevent) >= 11) {
      /* Require OpenCL 1.1 */
      cl_int ret = clSetEventCallback(clevent, CL_COMPLETE, opencl_event_callback, event);
      return (ret != CL_SUCCESS);
   }
   else {
      /* OpenCL1.0 */
      opencl_event_binding_t b = opencl_event_binding_new();
      b->event = event;
      b->clevent = clevent;

      pthread_mutex_lock(&blist_mutex);
      opencl_event_binding_list_push_back(blist, b);
      pthread_cond_signal(&blist_cond);
      pthread_mutex_unlock(&blist_mutex);

      return 0;
   }
}
