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

#ifndef __DRIVER_COMMON_H__
#define __DRIVER_COMMON_H__

#include <sys/time.h>
#include <starpu.h>
#include <starpu_profiling.h>
#include <core/jobs.h>
#include <profiling/profiling.h>
#include <common/utils.h>

typedef struct starpu_driver_t * starpu_driver;
typedef struct starpu_device_t * starpu_device;
typedef struct starpu_memory_node_t * starpu_memory_node;
typedef struct starpu_buffer_t * starpu_buffer;
typedef struct starpu_event_t * starpu_event;

struct starpu_driver_t {
   int id;
   int (*init)(starpu_driver);
   int (*getDevices)(starpu_driver, unsigned devices_size, starpu_device *devices, unsigned * num_devices);
   int (*release)(starpu_driver);
   void *data;
};

typedef enum starpu_device_info_t {
} starpu_device_info;

struct starpu_device_t {
   int (*getInfo)(starpu_device, starpu_device_info, size_t, void *, size_t *);
   int (*getMemoryNodes)(starpu_device, unsigned nodes_size, starpu_memory_node *nodes, unsigned *num_nodes);
};

struct starpu_memory_node_t {
   int (*createBuffer)(size_t size, starpu_buffer *buffer);
   int (*getBuffers)(unsigned *num_buffers, starpu_buffer **buffers);
};

struct starpu_buffer_t {
   int (*retain)(starpu_buffer);
   int (*release)(starpu_buffer);
   int (*getSize)(starpu_buffer, size_t*);
   int (*read)(starpu_buffer, size_t offset, size_t size, void * ptr, starpu_event *event);
   //TODO: clEnqueueReadBufferRect
   //int (*readRect)(starpu_buffer, size_t offset, size_t size, void * ptr, starpu_event *event);
   int (*write)(starpu_buffer, size_t offset, size_t size, void * ptr, starpu_event *event);
   //TODO: clEnqueueWriteBufferRect
   //int (*writeRect)(starpu_buffer, size_t offset, size_t size, void * ptr, starpu_event *event);
   int (*copy)(starpu_buffer, starpu_buffer, size_t src_offset, size_t dst_offset, size_t size, starpu_event *event);
   //TODO: clEnqueueCopyBufferRect
   //int (*copyRect)(starpu_buffer, size_t offset, size_t size, void * ptr, starpu_event *event);
};

void _starpu_driver_update_job_feedback(starpu_job_t j, struct starpu_worker_s *worker_args,
		unsigned calibrate_model,
		struct timespec *codelet_start, struct timespec *codelet_end);

void _starpu_block_worker(int workerid, pthread_cond_t *cond, pthread_mutex_t *mutex);

#endif // __DRIVER_COMMON_H__
