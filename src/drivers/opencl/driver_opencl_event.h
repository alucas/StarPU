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

#ifndef __DRIVER_OPENCL_EVENT_H__
#define __DRIVER_OPENCL_EVENT_H__

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef STARPU_USE_OPENCL

#include <CL/cl.h>

starpu_event _starpu_opencl_event_create(cl_event);

#endif // STARPU_USE_OPENCL
#endif //  __DRIVER_OPENCL_EVENT_H__
