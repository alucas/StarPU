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

#include <starpu.h>
#include <common/config.h>
#include <common/common.h>
#include <common/utils.h>
#include <core/sched_policy.h>
#include <datawizard/datastats.h>
#include <common/fxt.h>
#include <core/event.h>
#include "copy_driver.h"
#include "memalloc.h"
#include <starpu_opencl.h>
#include <starpu_cuda.h>
#include <profiling/profiling.h>

struct starpu_readwrite_buffer_args {
   void * interface;
   starpu_data_handle src_handle;
   starpu_data_handle dst_handle;
   starpu_event event;
   int direction;
};

void _starpu_wake_all_blocked_workers_on_node(unsigned nodeid)
{
	/* wake up all workers on that memory node */
	unsigned cond_id;

	starpu_mem_node_descr * const descr = _starpu_get_memory_node_description();

	PTHREAD_RWLOCK_RDLOCK(&descr->conditions_rwlock);

	unsigned nconds = descr->condition_count[nodeid];
	for (cond_id = 0; cond_id < nconds; cond_id++)
	{
		struct _cond_and_mutex *condition;
		condition  = &descr->conditions_attached_to_node[nodeid][cond_id];

		/* wake anybody waiting on that condition */
		PTHREAD_MUTEX_LOCK(condition->mutex);
		PTHREAD_COND_BROADCAST(condition->cond);
		PTHREAD_MUTEX_UNLOCK(condition->mutex);
	}

	PTHREAD_RWLOCK_UNLOCK(&descr->conditions_rwlock);
}

void starpu_wake_all_blocked_workers(void)
{
	/* workers may be blocked on the various queues' conditions */
	unsigned cond_id;

	starpu_mem_node_descr * const descr = _starpu_get_memory_node_description();

	PTHREAD_RWLOCK_RDLOCK(&descr->conditions_rwlock);

	unsigned nconds = descr->total_condition_count;
	for (cond_id = 0; cond_id < nconds; cond_id++)
	{
		struct _cond_and_mutex *condition;
		condition  = &descr->conditions_all[cond_id];

		/* wake anybody waiting on that condition */
		PTHREAD_MUTEX_LOCK(condition->mutex);
		PTHREAD_COND_BROADCAST(condition->cond);
		PTHREAD_MUTEX_UNLOCK(condition->mutex);
	}

	PTHREAD_RWLOCK_UNLOCK(&descr->conditions_rwlock);
}

#ifdef STARPU_USE_FXT
/* we need to identify each communication so that we can match the beginning
 * and the end of a communication in the trace, so we use a unique identifier
 * per communication */
static unsigned communication_cnt = 0;
#endif


static void enqueue_readwrite_callback_callback(void*data) {
   struct starpu_readwrite_buffer_args * arg = (struct starpu_readwrite_buffer_args*)data;
   _starpu_event_complete(arg->event);
   _starpu_event_release_private(arg->event);
   if (arg->direction)
      starpu_data_unregister(arg->dst_handle);
   else
      starpu_data_unregister(arg->src_handle);
   free(arg);
}

static void enqueue_readwrite_callback(void*data) {
   struct starpu_readwrite_buffer_args * arg = (struct starpu_readwrite_buffer_args*)data;


   if (arg->direction) {
      /* Read */
      int src_node = _starpu_select_src_node(arg->src_handle);
      starpu_data_request_t r = _starpu_create_data_request(arg->src_handle, src_node, arg->dst_handle, 0, src_node, STARPU_R, 0);
      _starpu_data_request_append_callback(r, enqueue_readwrite_callback_callback, arg);
      _starpu_post_data_request(r, src_node);
   }
   else {
      /* write */
      starpu_data_request_t r = _starpu_create_data_request(arg->dst_handle, 0, arg->src_handle, 0, 0, STARPU_W, 0);
      _starpu_data_request_append_callback(r, enqueue_readwrite_callback_callback, arg);
      _starpu_post_data_request(r, 0);
   }
}

//FIXME: we don't consider offset!!! We assume that ptr targets the same kind of data structure as handle
static int starpu_data_readwrite_buffer(starpu_data_handle handle, void*ptr, size_t offset, size_t size, int num_events, starpu_event *events, starpu_event *event, int direction){
   //FIXME
   assert(offset == 0);

   /* Create temporary variable data on host */
   starpu_data_handle handle2;
   starpu_variable_data_register(&handle2, 0, (uintptr_t)ptr, size);

   struct starpu_readwrite_buffer_args * arg = malloc(sizeof(struct starpu_readwrite_buffer_args));
   arg->direction = direction;

   if (direction) {
      arg->src_handle = handle;
      arg->dst_handle = handle2;
   }
   else {
      arg->src_handle = handle2;
      arg->dst_handle = handle;
   }

   arg->event = _starpu_event_create();

   if (event != NULL) {
      starpu_event_retain(arg->event);
      *event = arg->event;
   }

   /* We create a trigger that will post the request */
   starpu_trigger trigger = _starpu_trigger_create(enqueue_readwrite_callback, arg, NULL);
   _starpu_trigger_events_register(trigger, num_events, events);
   _starpu_trigger_enable(trigger);

   return 0;
}

int starpu_data_read_buffer(starpu_data_handle handle, void*ptr, size_t offset, size_t size, int num_events, starpu_event *events, starpu_event *event) {
   return starpu_data_readwrite_buffer(handle, ptr, offset, size, num_events, events, event, 1);
}

int starpu_data_write_buffer(starpu_data_handle handle, void*ptr, size_t offset, size_t size, int num_events, starpu_event *events, starpu_event *event) {
   return starpu_data_readwrite_buffer(handle, ptr, offset, size, num_events, events, event, 0);
}


static int copy_data_1_to_1_generic(starpu_data_handle src_handle, uint32_t src_node, starpu_data_handle dst_handle, uint32_t dst_node, struct starpu_data_request_s *req __attribute__((unused)))
{
	int ret = 0;

	const struct starpu_data_copy_methods *copy_methods = src_handle->ops->copy_methods;

	starpu_node_kind src_kind = _starpu_get_node_kind(src_node);
	starpu_node_kind dst_kind = _starpu_get_node_kind(dst_node);

	STARPU_ASSERT(src_handle->per_node[src_node].refcnt);
	STARPU_ASSERT(dst_handle->per_node[dst_node].refcnt);

	STARPU_ASSERT(src_handle->per_node[src_node].allocated);
	STARPU_ASSERT(dst_handle->per_node[dst_node].allocated);

#ifdef STARPU_USE_CUDA
	cudaError_t cures;
	cudaStream_t *stream;
#endif

	void *src_interface = starpu_data_get_interface_on_node(src_handle, src_node);
	void *dst_interface = starpu_data_get_interface_on_node(dst_handle, dst_node);

	switch (_STARPU_MEMORY_NODE_TUPLE(src_kind,dst_kind)) {
	case _STARPU_MEMORY_NODE_TUPLE(STARPU_CPU_RAM,STARPU_CPU_RAM):
		/* STARPU_CPU_RAM -> STARPU_CPU_RAM */
		STARPU_ASSERT(copy_methods->ram_to_ram);
		copy_methods->ram_to_ram(src_interface, src_node, dst_interface, dst_node);
		break;
#ifdef STARPU_USE_CUDA
	case _STARPU_MEMORY_NODE_TUPLE(STARPU_CUDA_RAM,STARPU_CPU_RAM):
		/* CUBLAS_RAM -> STARPU_CPU_RAM */
		/* only the proper CUBLAS thread can initiate this ! */
		if (_starpu_get_local_memory_node() == src_node) {
			/* only the proper CUBLAS thread can initiate this directly ! */
			STARPU_ASSERT(copy_methods->cuda_to_ram);
			if (!req || !copy_methods->cuda_to_ram_async) {
				/* this is not associated to a request so it's synchronous */
				copy_methods->cuda_to_ram(src_interface, src_node, dst_interface, dst_node);
			}
			else {
				cures = cudaEventCreate(&req->async_channel.cuda_event);
				if (STARPU_UNLIKELY(cures != cudaSuccess)) STARPU_CUDA_REPORT_ERROR(cures);

				stream = starpu_cuda_get_local_stream();
				ret = copy_methods->cuda_to_ram_async(src_interface, src_node, dst_interface, dst_node, stream);

				cures = cudaEventRecord(req->async_channel.cuda_event, *stream);
				if (STARPU_UNLIKELY(cures != cudaSuccess)) STARPU_CUDA_REPORT_ERROR(cures);
			}
		}
		else {
			/* we should not have a blocking call ! */
			STARPU_ABORT();
		}
		break;
	case _STARPU_MEMORY_NODE_TUPLE(STARPU_CPU_RAM,STARPU_CUDA_RAM):
		/* STARPU_CPU_RAM -> CUBLAS_RAM */
		/* only the proper CUBLAS thread can initiate this ! */
		STARPU_ASSERT(_starpu_get_local_memory_node() == dst_node);
		STARPU_ASSERT(copy_methods->ram_to_cuda);
		if (!req || !copy_methods->ram_to_cuda_async) {
			/* this is not associated to a request so it's synchronous */
			copy_methods->ram_to_cuda(src_interface, src_node, dst_interface, dst_node);
		}
		else {
			cures = cudaEventCreate(&req->async_channel.cuda_event);
			if (STARPU_UNLIKELY(cures != cudaSuccess)) STARPU_CUDA_REPORT_ERROR(cures);

			stream = starpu_cuda_get_local_stream();
			ret = copy_methods->ram_to_cuda_async(src_interface, src_node, dst_interface, dst_node, stream);

			cures = cudaEventRecord(req->async_channel.cuda_event, *stream);
			if (STARPU_UNLIKELY(cures != cudaSuccess)) STARPU_CUDA_REPORT_ERROR(cures);
		}
		break;
#endif
#ifdef STARPU_USE_OPENCL
	case _STARPU_MEMORY_NODE_TUPLE(STARPU_OPENCL_RAM,STARPU_CPU_RAM):
		/* OpenCL -> RAM */
		if (_starpu_get_local_memory_node() == src_node) {
			STARPU_ASSERT(copy_methods->opencl_to_ram);
			if (!req || !copy_methods->opencl_to_ram_async) {
				/* this is not associated to a request so it's synchronous */
				copy_methods->opencl_to_ram(src_interface, src_node, dst_interface, dst_node);
			}
			else {
            starpu_event event;
				ret = copy_methods->opencl_to_ram_async(src_interface, src_node, dst_interface, dst_node, &event);
            starpu_event_bind(event, req->event);
			}
		}
		else {
			/* we should not have a blocking call ! */
			STARPU_ABORT();
		}
		break;
	case _STARPU_MEMORY_NODE_TUPLE(STARPU_CPU_RAM,STARPU_OPENCL_RAM):
		/* STARPU_CPU_RAM -> STARPU_OPENCL_RAM */
		STARPU_ASSERT(_starpu_get_local_memory_node() == dst_node);
		STARPU_ASSERT(copy_methods->ram_to_opencl);
		if (!req || !copy_methods->ram_to_opencl_async) {
			/* this is not associated to a request so it's synchronous */
			copy_methods->ram_to_opencl(src_interface, src_node, dst_interface, dst_node);
		}
		else {
         starpu_event event;
			ret = copy_methods->ram_to_opencl_async(src_interface, src_node, dst_interface, dst_node, &event);
         starpu_event_bind(event, req->event);
		}
		break;
#endif
	default:
		STARPU_ABORT();
		break;
	}

	return ret;
}

int __attribute__((warn_unused_result)) _starpu_driver_copy_data_1_to_1(starpu_data_handle src_handle, uint32_t src_node, starpu_data_handle dst_handle,
		uint32_t dst_node, unsigned donotread, struct starpu_data_request_s *req, unsigned may_alloc)
{
	if (!donotread)
	{
		STARPU_ASSERT(src_handle->per_node[src_node].allocated);
		STARPU_ASSERT(src_handle->per_node[src_node].refcnt);
	}

	int ret_alloc, ret_copy;
	unsigned __attribute__((unused)) com_id = 0;

	/* first make sure the destination has an allocated buffer */
	ret_alloc = _starpu_allocate_memory_on_node(dst_handle, dst_node, may_alloc);
	if (ret_alloc)
		goto nomem;

	STARPU_ASSERT(dst_handle->per_node[dst_node].allocated);
	STARPU_ASSERT(dst_handle->per_node[dst_node].refcnt);

	/* if there is no need to actually read the data, 
	 * we do not perform any transfer */
	if (!donotread) {
		STARPU_ASSERT(src_handle->ops);
		//STARPU_ASSERT(handle->ops->copy_data_1_to_1);

		size_t size = _starpu_data_get_size(src_handle);
		_starpu_bus_update_profiling_info((int)src_node, (int)dst_node, size);
		
#ifdef STARPU_USE_FXT
		com_id = STARPU_ATOMIC_ADD(&communication_cnt, 1);

		if (req)
			req->com_id = com_id;
#endif

		/* for now we set the size to 0 in the FxT trace XXX */
		STARPU_TRACE_START_DRIVER_COPY(src_node, dst_node, 0, com_id);
		ret_copy = copy_data_1_to_1_generic(src_handle, src_node, dst_handle, dst_node, req);

#ifdef STARPU_USE_FXT
		if (ret_copy != EAGAIN)
		{
			size_t size = _starpu_data_get_size(src_handle);
			STARPU_TRACE_END_DRIVER_COPY(src_node, dst_node, size, com_id);
		}
#endif

		return ret_copy;
	}

	return 0;

nomem:
	return ENOMEM;
}

void _starpu_driver_wait_request_completion(starpu_data_request_t req __attribute__ ((unused)),
					unsigned handling_node)
{
	starpu_node_kind kind = _starpu_get_node_kind(handling_node);
#ifdef STARPU_USE_CUDA
	cudaEvent_t event;
	cudaError_t cures;
#endif

	switch (kind) {
#ifdef STARPU_USE_CUDA
		case STARPU_CUDA_RAM:
			event = req->async_channel.cuda_event;

			cures = cudaEventSynchronize(event);
			if (STARPU_UNLIKELY(cures))
				STARPU_CUDA_REPORT_ERROR(cures);

			cures = cudaEventDestroy(event);
			if (STARPU_UNLIKELY(cures))
				STARPU_CUDA_REPORT_ERROR(cures);

			break;
#endif
#ifdef STARPU_USE_OPENCL
      case STARPU_OPENCL_RAM:
         starpu_event_wait(req->event);
         break;
#endif
		case STARPU_CPU_RAM:
		default:
			STARPU_ABORT();
	}
}

unsigned _starpu_driver_test_request_completion(starpu_data_request_t req __attribute__ ((unused)),
					unsigned handling_node)
{
	starpu_node_kind kind = _starpu_get_node_kind(handling_node);
	unsigned success = 0;
#ifdef STARPU_USE_CUDA
	cudaEvent_t event;
#endif

   switch (kind) {
#ifdef STARPU_USE_CUDA
      case STARPU_CUDA_RAM:
         event = req->async_channel.cuda_event;

         success = (cudaEventQuery(event) == cudaSuccess);
         if (success)
            cudaEventDestroy(event);

         break;
#endif
#ifdef STARPU_USE_OPENCL
      case STARPU_OPENCL_RAM:
         success = starpu_event_test(req->event);
         break;
#endif
      case STARPU_CPU_RAM:
      default:
         STARPU_ABORT();
         success = 0;
   }

	return success;
}
