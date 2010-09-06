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
#include <drivers/opencl/event.h>

static void opencl_event_callback(cl_event event, cl_int status, void *user_data) {
   starpu_event ev = (starpu_event)user_data;

   _starpu_event_complete(ev);

   clReleaseEvent(event);
}

starpu_event _starpu_opencl_event_create(cl_event event) {

   starpu_event ev = _starpu_event_create();

   clSetEventCallback(event, CL_COMPLETE, opencl_event_callback, ev);

   return ev;
}

#endif // STARPU_USE_OPENCL
