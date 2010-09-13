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

#ifndef __OPENCL_H__
#define __OPENCL_H__

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <CL/cl.h>

int _starpu_opencl_init_context(int devid);

int _starpu_opencl_deinit_context(int devid);

unsigned _starpu_opencl_get_device_count(void);

int _starpu_opencl_allocate_memory(void **addr, size_t size, cl_mem_flags flags);

int _starpu_opencl_copy_ram_to_opencl(void *ptr, cl_mem buffer, size_t size, size_t offset, cl_event *event);

int _starpu_opencl_copy_opencl_to_ram(cl_mem buffer, void *ptr, size_t size, size_t offset, cl_event *event);

int _starpu_opencl_copy_ram_to_opencl_async_sync(void *ptr, cl_mem buffer, size_t size, size_t offset, starpu_event *event, int *ret);

int _starpu_opencl_copy_opencl_to_ram_async_sync(cl_mem buffer, void *ptr, size_t size, size_t offset, starpu_event *event, int *ret);

void _starpu_opencl_init(void);

void *_starpu_opencl_worker(void *);

#endif //  __OPENCL_H__
