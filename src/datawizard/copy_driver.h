/*
 * StarPU
 * Copyright (C) INRIA 2008-2009 (see AUTHORS file)
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

#ifndef __COPY_DRIVER_H__
#define __COPY_DRIVER_H__

#include <common/config.h>
#include <datawizard/memory_nodes.h>
#include "coherency.h"
#include "memalloc.h"

#ifdef STARPU_USE_CUDA
#include <cuda.h>
#include <cuda_runtime.h>
#include <cublas.h>
#endif

#ifdef STARPU_USE_OPENCL
#include <CL/cl.h>
#endif

struct starpu_data_request_s;

/* this is a structure that can be queried to see whether an asynchronous
 * transfer has terminated or not */
typedef union {
	int dummy;
#ifdef STARPU_USE_CUDA
	cudaEvent_t cuda_event;
#endif
#ifdef STARPU_USE_OPENCL
        cl_event opencl_event;
#endif
} starpu_async_channel;

struct starpu_copy_data_methods_s {
	/* src type is ram */
	int (*ram_to_ram)(starpu_data_handle handle, starpu_memory_node src, starpu_memory_node dst);
	int (*ram_to_cuda)(starpu_data_handle handle, starpu_memory_node src, starpu_memory_node dst);
	int (*ram_to_opencl)(starpu_data_handle handle, starpu_memory_node src, starpu_memory_node dst);
	int (*ram_to_spu)(starpu_data_handle handle, starpu_memory_node src, starpu_memory_node dst);

	/* src type is cuda */
	int (*cuda_to_ram)(starpu_data_handle handle, starpu_memory_node src, starpu_memory_node dst);
	int (*cuda_to_cuda)(starpu_data_handle handle, starpu_memory_node src, starpu_memory_node dst);
	int (*cuda_to_opencl)(starpu_data_handle handle, starpu_memory_node src, starpu_memory_node dst);
	int (*cuda_to_spu)(starpu_data_handle handle, starpu_memory_node src, starpu_memory_node dst);

	/* src type is spu */
	int (*spu_to_ram)(starpu_data_handle handle, starpu_memory_node src, starpu_memory_node dst);
	int (*spu_to_cuda)(starpu_data_handle handle, starpu_memory_node src, starpu_memory_node dst);
	int (*spu_to_opencl)(starpu_data_handle handle, starpu_memory_node src, starpu_memory_node dst);
	int (*spu_to_spu)(starpu_data_handle handle, starpu_memory_node src, starpu_memory_node dst);

	/* src type is opencl */
	int (*opencl_to_ram)(starpu_data_handle handle, starpu_memory_node src, starpu_memory_node dst);
	int (*opencl_to_cuda)(starpu_data_handle handle, starpu_memory_node src, starpu_memory_node dst);
	int (*opencl_to_opencl)(starpu_data_handle handle, starpu_memory_node src, starpu_memory_node dst);
	int (*opencl_to_spu)(starpu_data_handle handle, starpu_memory_node src, starpu_memory_node dst);

#ifdef STARPU_USE_CUDA
	/* for asynchronous CUDA transfers */
	int (*ram_to_cuda_async)(starpu_data_handle handle, starpu_memory_node src,
					starpu_memory_node dst, cudaStream_t *stream);
	int (*cuda_to_ram_async)(starpu_data_handle handle, starpu_memory_node src,
					starpu_memory_node dst, cudaStream_t *stream);
	int (*cuda_to_cuda_async)(starpu_data_handle handle, starpu_memory_node src,
					starpu_memory_node dst, cudaStream_t *stream);
#endif

#ifdef STARPU_USE_OPENCL
	/* for asynchronous OpenCL transfers */
        int (*ram_to_opencl_async)(starpu_data_handle handle, starpu_memory_node src, starpu_memory_node dst, cl_event *event);
	int (*opencl_to_ram_async)(starpu_data_handle handle, starpu_memory_node src, starpu_memory_node dst, cl_event *event);
	int (*opencl_to_opencl_async)(starpu_data_handle handle, starpu_memory_node src, starpu_memory_node dst, cl_event *event);
#endif
};

void _starpu_wake_all_blocked_workers_on_node(starpu_memory_node node);

__attribute__((warn_unused_result))
int _starpu_driver_copy_data_1_to_1(starpu_data_handle handle, starpu_memory_node node, 
		starpu_memory_node requesting_node, unsigned donotread, struct starpu_data_request_s *req, unsigned may_allloc);

unsigned _starpu_driver_test_request_completion(starpu_async_channel *async_channel, starpu_memory_node handling_node);
void _starpu_driver_wait_request_completion(starpu_async_channel *async_channel, starpu_memory_node handling_node);
#endif // __COPY_DRIVER_H__
