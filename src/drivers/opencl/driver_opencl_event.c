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

#ifdef STARPU_USE_OPENCL

#include <CL/cl.h>
#include <starpu_event.h>
#include <core/event.h>
#include "driver_opencl_event.h"

#define CLCHECK(f) assert(CL_SUCCESS == (f));

static int _starpu_opencl_event_init(starpu_event);
static int _starpu_opencl_event_wait(starpu_event);
static int _starpu_opencl_event_free(starpu_event);
static starpu_event_status_t _starpu_opencl_event_status(starpu_event);

static struct starpu_event_methods_t methods = {
   .wait = _starpu_opencl_event_wait,
   .free = _starpu_opencl_event_free,
   .status = _starpu_opencl_event_status
};

starpu_event _starpu_opencl_event_create(cl_event event) {
   return _starpu_event_create(&methods, event);
}

static int _starpu_opencl_event_wait(starpu_event event) {
   cl_event ev = (cl_event)_starpu_event_data(event);
   CLCHECK((err = clWaitForEvents(1, &ev)));

   return (err == CL_SUCCESS ? 0 : 1);
}

static int _starpu_opencl_event_free(starpu_event event) {
   cl_event ev = (cl_event)_starpu_event_data(event);
   CLCHECK((err = clReleaseEvent(ev)));

   return (err == CL_SUCCESS ? 0 : 1);
}

static starpu_event_status_t _starpu_opencl_event_status(starpu_event event) {
   cl_event ev = (cl_event)_starpu_event_data(event);
   cl_int status;
   CLCHECK(clGetEventInfo(ev, Cl_EVENT_COMMAND_EXECUTION_STATUS, sizeof(cl_int), &status, NULL));
   switch (status) {
      case CL_QUEUED:
      case CL_SUBMITTED:
         return STARPU_EVENT_WAITING;
      case CL_RUNNING:
         return STARPU_EVENT_RUNNING;
      case CL_COMPLETE:
         return STARPU_EVENT_COMPLETE;
   }
}

#undef CLCHECK

#endif // STARPU_USE_OPENCL
