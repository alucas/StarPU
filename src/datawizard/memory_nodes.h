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

#ifndef __MEMORY_NODES_H__
#define __MEMORY_NODES_H__

#include <starpu.h>
#include <common/config.h>
#include <common/decls.h>

#include <datawizard/coherency.h>
#include <datawizard/memalloc.h>
#include <core/mechanisms/queues.h>

typedef enum {
   STARPU_UNUSED     = 0x00,
	STARPU_CPU_RAM    = 0x01,
	STARPU_CUDA_RAM   = 0x02,
   STARPU_OPENCL_RAM = 0x03,
	STARPU_SPU_LS     = 0x04
} starpu_node_kind;

typedef starpu_node_kind starpu_memory_node_tuple;

#define MEMORY_NODE_TUPLE(node1,node2) (node1 | (node2 << 4))
#define MEMORY_NODE_TUPLE_FIRST(tuple) (tuple & 0x0F)
#define MEMORY_NODE_TUPLE_SECOND(tuple) (tuple & 0xF0)

#define DEFAULTSIZE 8
// XXX 32 is set randomly !
#define NUM_QUEUES 32

struct starpu_memory_node_t {
   /* We use an ID which is an index for per_node in data_handle as
      well as an index in nodes array. This field will be deleted when
      per_node will be deleted */
   unsigned int id;

   starpu_node_kind kind;
   starpu_job_queue queues[NUM_QUEUES];
   unsigned int queue_count;

   /* requests that have not been treated at all */
   starpu_data_request_list_t data_requests;
   pthread_cond_t data_requests_list_cond;
   pthread_mutex_t data_requests_list_mutex;

   /* requests that are not terminated (eg. async transfers) */
   starpu_data_request_list_t data_requests_pending;
   pthread_cond_t data_requests_pending_list_cond;
   pthread_mutex_t data_requests_pending_list_mutex;
};

void starpu_memory_nodes_readlock();
void starpu_memory_nodes_writelock();
void starpu_memory_nodes_unlock();

PURE unsigned int starpu_memory_nodes_count();
PURE starpu_memory_node starpu_memory_nodes_get(unsigned int id);

PURE unsigned int starpu_memory_nodes_queue_count();
starpu_job_queue starpu_memory_nodes_queue_get(unsigned int index);
void starpu_memory_nodes_queue_add(starpu_job_queue q);

PURE unsigned int starpu_memory_node_queue_count(starpu_memory_node node);
starpu_job_queue starpu_memory_node_queue_get(starpu_memory_node node, unsigned int index);
void starpu_memory_node_queue_add(starpu_memory_node node, starpu_job_queue q);

PURE starpu_node_kind starpu_memory_node_kind(starpu_memory_node node);

void _starpu_init_memory_nodes(void);
void _starpu_deinit_memory_nodes(void);
void _starpu_set_local_memory_node_key(starpu_memory_node *node);
starpu_memory_node _starpu_get_local_memory_node(void);
starpu_memory_node _starpu_register_memory_node(starpu_node_kind kind);
void _starpu_memory_node_attach_queue(struct starpu_jobq_s *q, starpu_memory_node node);


#endif // __MEMORY_NODES_H__
