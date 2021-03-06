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

#ifndef __DATA_REQUEST_H__
#define __DATA_REQUEST_H__

LIST_DECLARE_TYPE(starpu_data_request);

#include <semaphore.h>
#include <datawizard/coherency.h>
#include <common/list.h>
#include <datawizard/copy_driver.h>
#include <common/starpu_spinlock.h>

struct callback_list {
	void (*callback_func)(void *);
	void *callback_arg;
	struct callback_list *next;
};

LIST_CREATE_TYPE(starpu_data_request,
	starpu_spinlock_t lock;
	unsigned refcnt;

   starpu_event event;

	starpu_data_handle src_handle;
   uint32_t src_node;
	starpu_data_handle dst_handle;
   uint32_t dst_node;

	uint32_t handling_node;

	starpu_access_mode mode;

	starpu_async_channel async_channel;

	unsigned completed;
	int retval;

	/* in case we have a chain of request (eg. for nvidia multi-GPU) */
	struct starpu_data_request_s *next_req[STARPU_MAXNODES];
	/* who should perform the next request ? */
	unsigned next_req_count;

	struct callback_list *callbacks;

	unsigned is_a_prefetch_request;

#ifdef STARPU_USE_FXT
	unsigned com_id;
#endif
);

/* Everyone that wants to access some piece of data will post a request.
 * Not only StarPU internals, but also the application may put such requests */
LIST_TYPE(starpu_data_requester,
	/* what kind of access is requested ? */
	starpu_access_mode mode;

	/* applications may also directly manipulate data */
	unsigned is_requested_by_codelet;

	/* in case this is a codelet that will do the access */
	struct starpu_job_s *j;
	unsigned buffer_index;

	/* if this is more complicated ... (eg. application request) 
	 * NB: this callback is not called with the lock taken !
	 */
	void (*ready_data_callback)(void *argcb);
	void *argcb;
);

void _starpu_init_data_request_lists(void);
void _starpu_deinit_data_request_lists(void);
void _starpu_post_data_request(starpu_data_request_t r, uint32_t handling_node);
void _starpu_handle_node_data_requests(uint32_t src_node, unsigned may_alloc);

void _starpu_handle_pending_node_data_requests(uint32_t src_node);
void _starpu_handle_all_pending_node_data_requests(uint32_t src_node);

int _starpu_check_that_no_data_request_exists(uint32_t node);

starpu_data_request_t _starpu_create_data_request(starpu_data_handle src_handle, uint32_t src_node, starpu_data_handle dst_handle, uint32_t dst_node, uint32_t handling_node, starpu_access_mode mode, unsigned is_prefetch);
starpu_data_request_t _starpu_search_existing_data_request(starpu_data_handle handle, uint32_t dst_node, starpu_access_mode mode);
int _starpu_wait_data_request_completion(starpu_data_request_t r, unsigned may_alloc);

void _starpu_data_request_append_callback(starpu_data_request_t r,
			void (*callback_func)(void *), void *callback_arg);

#endif // __DATA_REQUEST_H__
