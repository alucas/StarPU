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

#ifndef __COPY_DRIVER_H__
#define __COPY_DRIVER_H__

#include <common/config.h>
#include <common/list.h>
#include <datawizard/memory_nodes.h>
#include "coherency.h"
#include "memalloc.h"

#ifndef __DATA_REQUEST_H__
LIST_DECLARE_TYPE(starpu_data_request);
#endif

#ifdef STARPU_USE_CUDA
#include <cuda.h>
#include <cuda_runtime.h>
#include <cublas.h>
#endif

struct starpu_data_request_s;

/* this is a structure that can be queried to see whether an asynchronous
 * transfer has terminated or not */
typedef union {
	int dummy;
#ifdef STARPU_USE_CUDA
	cudaEvent_t cuda_event;
#endif
} starpu_async_channel;

void _starpu_wake_all_blocked_workers_on_node(unsigned nodeid);

__attribute__((warn_unused_result))
int _starpu_driver_copy_data_1_to_1(starpu_data_handle src_handle, uint32_t src_node, 
                                    starpu_data_handle dst_handle, uint32_t dst_node,
		unsigned donotread, struct starpu_data_request_s *req, unsigned may_allloc);

unsigned _starpu_driver_test_request_completion(starpu_data_request_t req, unsigned handling_node);
void _starpu_driver_wait_request_completion(starpu_data_request_t req, unsigned handling_node);
#endif // __COPY_DRIVER_H__
