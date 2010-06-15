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

#include <pthread.h>
#include <common/config.h>
#include <core/policies/sched_policy.h>
#include <datawizard/datastats.h>
#include <common/fxt.h>
#include "copy_driver.h"
#include "memalloc.h"

#define NODE(n) nodes[n->id]

static struct starpu_memory_node_t * nodes;
static unsigned int node_count = 0;
static unsigned int node_size = 0;

static starpu_job_queue *all_queues;
static unsigned all_queue_count = 0;
static unsigned all_queue_size = 0;

pthread_rwlock_t attached_queues_rwlock;
static pthread_key_t memory_node_key;

PURE unsigned int starpu_memory_nodes_count() {
   return node_count;
}

void starpu_memory_node_init(struct starpu_memory_node_t *node) {
   unsigned int j;

   for (j=0; j<NUM_QUEUES;j++) {
      node->queue_count = 0;
      node->kind = STARPU_UNUSED;
   }

   node->id = (node - nodes) / sizeof(struct starpu_memory_node_t);

   node->data_requests = starpu_data_request_list_new();
   PTHREAD_MUTEX_INIT(&node->data_requests_list_mutex, NULL);
   PTHREAD_COND_INIT(&node->data_requests_list_cond, NULL);

   node->data_requests_pending = starpu_data_request_list_new();
   PTHREAD_MUTEX_INIT(&node->data_requests_pending_list_mutex, NULL);
   PTHREAD_COND_INIT(&node->data_requests_pending_list_cond, NULL);
}

void starpu_memory_node_release(struct starpu_memory_node_t *node) {
   PTHREAD_COND_DESTROY(&node->data_requests_pending_list_cond);
   PTHREAD_MUTEX_DESTROY(&node->data_requests_pending_list_mutex);
   starpu_data_request_list_delete(node->data_requests_pending);

   PTHREAD_COND_DESTROY(&node->data_requests_list_cond);
   PTHREAD_MUTEX_DESTROY(&node->data_requests_list_mutex);
   starpu_data_request_list_delete(node->data_requests);
}


void _starpu_init_memory_nodes(void)
{
   unsigned int i;
   node_size = DEFAULTSIZE;

   nodes = malloc(sizeof(struct starpu_memory_node_t) * node_size);
   for (i=0; i<DEFAULTSIZE; i++)
      starpu_memory_node_init(&nodes[i]);

   all_queues = malloc(sizeof(starpu_job_queue) * NUM_QUEUES);
   all_queue_size = NUM_QUEUES;
   all_queue_count = 0;


	pthread_key_create(&memory_node_key, NULL);

	_starpu_init_mem_chunk_lists();
	_starpu_init_data_request_lists();

	pthread_rwlock_init(&attached_queues_rwlock, NULL);
}

void _starpu_deinit_memory_nodes(void)
{
	_starpu_deinit_data_request_lists();
	_starpu_deinit_mem_chunk_lists();

	pthread_key_delete(memory_node_key);
}

PURE starpu_memory_node starpu_memory_nodes_get(unsigned int id) {
   return &nodes[id];
}

PURE unsigned int starpu_memory_nodes_queue_count() {
   return all_queue_count;
}

starpu_job_queue starpu_memory_nodes_queue_get(unsigned int index) {
   return all_queues[index];
}

void starpu_memory_nodes_queue_add(starpu_job_queue q) {
   if (all_queue_count == all_queue_size) {
      all_queue_size *= 2;
      all_queues = realloc(all_queues, sizeof(starpu_job_queue) * all_queue_size);
   }

	all_queues[all_queue_count] = q;
	all_queue_count++;
}

PURE unsigned int starpu_memory_node_queue_count(starpu_memory_node node) {
   return NODE(node).queue_count;
}

void starpu_memory_nodes_readlock() {
	PTHREAD_RWLOCK_RDLOCK(&attached_queues_rwlock);
}

void starpu_memory_nodes_writelock() {
	pthread_rwlock_wrlock(&attached_queues_rwlock);
}

void starpu_memory_nodes_unlock() {
	PTHREAD_RWLOCK_UNLOCK(&attached_queues_rwlock);
}


starpu_job_queue starpu_memory_node_queue_get(starpu_memory_node node, unsigned int index) {
   return NODE(node).queues[index];
}

void starpu_memory_node_queue_add(starpu_memory_node node, starpu_job_queue q) {
   unsigned int index = starpu_memory_node_queue_count(node);
   NODE(node).queues[index] = q;
   NODE(node).queue_count++;
}

void _starpu_set_local_memory_node_key(starpu_memory_node *node)
{
	pthread_setspecific(memory_node_key, node);
}

starpu_memory_node _starpu_get_local_memory_node(void)
{
	starpu_memory_node *memory_node;
	memory_node = pthread_getspecific(memory_node_key);
	
	/* in case this is called by the programmer, we assume the RAM node 
	   is the appropriate memory node ... so we return 0 XXX */
	if (STARPU_UNLIKELY(!memory_node))
		return 0;

	return *memory_node;
}

PURE starpu_node_kind starpu_memory_node_kind(starpu_memory_node node) {
	return NODE(node).kind;
}

starpu_memory_node _starpu_register_memory_node(starpu_node_kind kind)
{
   if (node_count == node_size) {
      node_size *= 2;
      nodes = realloc(nodes, sizeof(struct starpu_memory_node_t) * node_size);

      unsigned int i;
      for (i=node_count; i<node_size; i++)
         starpu_memory_node_init(&nodes[i]);
   }

	node_count++;
	nodes[node_count-1].kind = kind;
	STARPU_TRACE_NEW_MEM_NODE(node_count-1);

	return &nodes[node_count-1];
}

/* TODO move in a more appropriate file  !! */
/* attach a queue to a memory node (if it's not already attached) */
void _starpu_memory_node_attach_queue(struct starpu_jobq_s *q, starpu_memory_node node)
{
	unsigned queue;
	unsigned nqueues_total, nqueues;
	
   starpu_memory_nodes_writelock();

	/* we only insert the queue if it's not already in the list */
	nqueues = starpu_memory_node_queue_count(node);
	for (queue = 0; queue < nqueues; queue++)
	{
		if (starpu_memory_node_queue_get(node,queue) == q)
		{
			/* the queue is already in the list */
         starpu_memory_nodes_unlock();
			return;
		}
	}

	/* it was not found locally */
   starpu_memory_node_queue_add(node, q);

	/* do we have to add it in the global list as well ? */
	nqueues_total = starpu_memory_nodes_queue_count(); 
	for (queue = 0; queue < nqueues_total; queue++)
	{
		if (starpu_memory_nodes_queue_get(queue) == q)
		{
			/* the queue is already in the global list */
         starpu_memory_nodes_unlock();
			return;
		}
	} 

	/* it was not in the global queue either */
   starpu_memory_nodes_queue_add(q);

   starpu_memory_nodes_unlock();
}

starpu_memory_node starpu_worker_get_memory_node(unsigned workerid)
{
	struct starpu_worker_s *worker = _starpu_get_worker_struct(workerid);

	return worker->memory_node;
}
