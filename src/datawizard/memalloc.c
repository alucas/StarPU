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

#include <datawizard/memalloc.h>
#include <datawizard/footprint.h>

static pthread_rwlock_t mc_rwlock[STARPU_MAXNODES]; 
static starpu_mem_chunk_list_t mc_list[STARPU_MAXNODES];
static starpu_mem_chunk_list_t memchunk_cache[STARPU_MAXNODES];

static size_t free_memory_on_node(starpu_mem_chunk_t mc, uint32_t node);

void _starpu_init_mem_chunk_lists(void)
{
	unsigned i;
	for (i = 0; i < STARPU_MAXNODES; i++)
	{
		pthread_rwlock_init(&mc_rwlock[i], NULL);
		mc_list[i] = starpu_mem_chunk_list_new();
		memchunk_cache[i] = starpu_mem_chunk_list_new();
	}
}

void _starpu_deinit_mem_chunk_lists(void)
{
	unsigned i;
	for (i = 0; i < STARPU_MAXNODES; i++)
	{
		starpu_mem_chunk_list_delete(mc_list[i]);
		starpu_mem_chunk_list_delete(memchunk_cache[i]);
	}
}

static void lock_all_subtree(starpu_data_handle handle)
{
	if (handle->nchildren == 0)
	{
		/* this is a leaf */
		while (_starpu_spin_trylock(&handle->header_lock))
			_starpu_datawizard_progress(_starpu_get_local_memory_node(), 0);
	}
	else {
		/* lock all sub-subtrees children */
		unsigned child;
		for (child = 0; child < handle->nchildren; child++)
		{
			lock_all_subtree(&handle->children[child]);
		}
	}
}

static void unlock_all_subtree(starpu_data_handle handle)
{
	if (handle->nchildren == 0)
	{
		/* this is a leaf */	
		_starpu_spin_unlock(&handle->header_lock);
	}
	else {
		/* lock all sub-subtrees children 
		 * Note that this is done in the reverse order of the
		 * lock_all_subtree so that we avoid deadlock */
		unsigned i;
		for (i =0; i < handle->nchildren; i++)
		{
			unsigned child = handle->nchildren - 1 - i;
			unlock_all_subtree(&handle->children[child]);
		}
	}
}

static unsigned may_free_subtree(starpu_data_handle handle, unsigned node)
{
	/* we only free if no one refers to the leaf */
	uint32_t refcnt = _starpu_get_data_refcnt(handle, node);
	if (refcnt)
		return 0;
	
	if (!handle->nchildren)
		return 1;
	
	/* look into all sub-subtrees children */
	unsigned child;
	for (child = 0; child < handle->nchildren; child++)
	{
		unsigned res;
		res = may_free_subtree(&handle->children[child], node);
		if (!res) return 0;
	}

	/* no problem was found */
	return 1;
}

static size_t do_free_mem_chunk(starpu_mem_chunk_t mc, unsigned node)
{
	size_t size;

	/* free the actual buffer */
	size = free_memory_on_node(mc, node);

	/* remove the mem_chunk from the list */
	starpu_mem_chunk_list_erase(mc_list[node], mc);

	free(mc->interface);
	starpu_mem_chunk_delete(mc);

	return size; 
}

static void transfer_subtree_to_node(starpu_data_handle handle, unsigned src_node, 
						unsigned dst_node)
{
	unsigned i;
	unsigned last = 0;
	unsigned cnt;
	int ret;

	if (handle->nchildren == 0)
	{
		/* this is a leaf */
		switch(handle->per_node[src_node].state) {
		case STARPU_OWNER:
			/* the local node has the only copy */
			/* the owner is now the destination_node */
			handle->per_node[src_node].state = STARPU_INVALID;
			handle->per_node[dst_node].state = STARPU_OWNER;

//FIXME: we should use requests during memory reclaim
			/* TODO use request !! */
			handle->per_node[src_node].refcnt++;
			handle->per_node[dst_node].refcnt++;

			ret = _starpu_driver_copy_data_1_to_1(handle, src_node, handle, dst_node, 0, NULL, 1);
			STARPU_ASSERT(ret == 0);

			handle->per_node[src_node].refcnt--;
			handle->per_node[dst_node].refcnt--;

			break;
		case STARPU_SHARED:
			/* some other node may have the copy */
			handle->per_node[src_node].state = STARPU_INVALID;

			/* count the number of copies */
			cnt = 0;
			for (i = 0; i < STARPU_MAXNODES; i++)
			{
				if (handle->per_node[i].state == STARPU_SHARED) {
					cnt++; 
					last = i;
				}
			}

			if (cnt == 1)
				handle->per_node[last].state = STARPU_OWNER;

			break;
		case STARPU_INVALID:
			/* nothing to be done */
			break;
		default:
			STARPU_ABORT();
			break;
		}
	}
	else {
		/* lock all sub-subtrees children */
		unsigned child;
		for (child = 0; child < handle->nchildren; child++)
		{
			transfer_subtree_to_node(&handle->children[child],
							src_node, dst_node);
		}
	}
}

static size_t try_to_free_mem_chunk(starpu_mem_chunk_t mc, unsigned node)
{
	size_t freed = 0;

	starpu_data_handle handle;

	handle = mc->data;

	STARPU_ASSERT(handle);

	/* try to lock all the leafs of the subtree */
	lock_all_subtree(handle);

	/* check if they are all "free" */
	if (may_free_subtree(handle, node))
	{
		STARPU_ASSERT(handle->per_node[node].refcnt == 0);

		/* in case there was nobody using that buffer, throw it 
		 * away after writing it back to main memory */
		transfer_subtree_to_node(handle, node, 0);

		STARPU_ASSERT(handle->per_node[node].refcnt == 0);

		/* now the actual buffer may be freed */
		freed = do_free_mem_chunk(mc, node);
	}

	/* unlock the leafs */
	unlock_all_subtree(handle);

	return freed;
}

#ifdef STARPU_USE_ALLOCATION_CACHE
/* we assume that mc_rwlock[node] is taken */
static void reuse_mem_chunk(unsigned node, starpu_data_handle new_data, starpu_mem_chunk_t mc, unsigned is_already_in_mc_list)
{
	starpu_data_handle old_data;
	old_data = mc->data;

	/* we found an appropriate mem chunk: so we get it out
	 * of the "to free" list, and reassign it to the new
	 * piece of data */

	if (!is_already_in_mc_list)
	{
		starpu_mem_chunk_list_erase(memchunk_cache[node], mc);
	}

	if (!mc->data_was_deleted)
	{
		old_data->per_node[node].allocated = 0;
		old_data->per_node[node].automatically_allocated = 0;
	}

	new_data->per_node[node].allocated = 1;
	new_data->per_node[node].automatically_allocated = 1;

	memcpy(&new_data->interface[node], mc->interface, old_data->ops->interface_size);

	mc->data = new_data;
	mc->data_was_deleted = 0;
	/* mc->ops, mc->size, mc->footprint and mc->interface should be
 	 * unchanged ! */
	
	/* reinsert the mem chunk in the list of active memory chunks */
	if (!is_already_in_mc_list)
	{
		starpu_mem_chunk_list_push_front(mc_list[node], mc);
	}
}

static unsigned try_to_reuse_mem_chunk(starpu_mem_chunk_t mc, unsigned node, starpu_data_handle new_data, unsigned is_already_in_mc_list)
{
	unsigned success = 0;

	starpu_data_handle old_data;

	old_data = mc->data;

	STARPU_ASSERT(old_data);

	/* try to lock all the leafs of the subtree */
	lock_all_subtree(old_data);

	/* check if they are all "free" */
	if (may_free_subtree(old_data, node))
	{
		success = 1;

		/* in case there was nobody using that buffer, throw it 
		 * away after writing it back to main memory */
		transfer_subtree_to_node(old_data, node, 0);

		/* now replace the previous data */
		reuse_mem_chunk(node, new_data, mc, is_already_in_mc_list);
	}

	/* unlock the leafs */
	unlock_all_subtree(old_data);

	return success;
}

/* this function looks for a memory chunk that matches a given footprint in the
 * list of mem chunk that need to be freed */
static unsigned try_to_find_reusable_mem_chunk(unsigned node, starpu_data_handle data, uint32_t footprint)
{
	pthread_rwlock_wrlock(&mc_rwlock[node]);

	/* go through all buffers in the cache */
	starpu_mem_chunk_t mc, next_mc;
	for (mc = starpu_mem_chunk_list_begin(memchunk_cache[node]);
	     mc != starpu_mem_chunk_list_end(memchunk_cache[node]);
	     mc = next_mc)
	{
		next_mc = starpu_mem_chunk_list_next(mc);

		if (mc->footprint == footprint)
		{

			starpu_data_handle old_data;
			old_data = mc->data;

			if (old_data->per_node[node].allocated &&
					old_data->per_node[node].automatically_allocated)
			{
				reuse_mem_chunk(node, data, mc, 0);

				pthread_rwlock_unlock(&mc_rwlock[node]);
				return 1;
			}
		}

	}

	/* now look for some non essential data in the active list */
	for (mc = starpu_mem_chunk_list_begin(mc_list[node]);
	     mc != starpu_mem_chunk_list_end(mc_list[node]);
	     mc = next_mc)
	{
		/* there is a risk that the memory chunk is freed before next
		 * iteration starts: so we compute the next element of the list
		 * now */
		next_mc = starpu_mem_chunk_list_next(mc);

		if (mc->data->is_not_important && (mc->footprint == footprint))
		{
//			fprintf(stderr, "found a candidate ...\n");
			if (try_to_reuse_mem_chunk(mc, node, data, 1))
			{
				pthread_rwlock_unlock(&mc_rwlock[node]);
				return 1;
			}
		}
	}

	pthread_rwlock_unlock(&mc_rwlock[node]);

	return 0;
}
#endif

static int _starpu_data_interface_compare(void *interface_a, struct starpu_data_interface_ops_t *ops_a,
						void *interface_b, struct starpu_data_interface_ops_t *ops_b)
{
	if (ops_a->interfaceid != ops_b->interfaceid)
		return -1;

	int ret = ops_a->compare(interface_a, interface_b);

	return ret;
}

starpu_mem_chunk_t _starpu_memchunk_cache_lookup(uint32_t node, starpu_data_handle handle)
{
	uint32_t footprint = _starpu_compute_data_footprint(handle);

	pthread_rwlock_wrlock(&mc_rwlock[node]);

	/* go through all buffers in the cache */
	starpu_mem_chunk_t mc;
	for (mc = starpu_mem_chunk_list_begin(memchunk_cache[node]);
	     mc != starpu_mem_chunk_list_end(memchunk_cache[node]);
	     mc = starpu_mem_chunk_list_next(mc))
	{
		if (mc->footprint == footprint)
		{
			/* Is that a false hit ? (this is _very_ unlikely) */
			if (_starpu_data_interface_compare(&handle->interface[node], handle->ops, mc->interface, mc->ops))
				continue;

			/* Cache hit */

			/* Remove from the cache */
			starpu_mem_chunk_list_erase(memchunk_cache[node], mc);
			pthread_rwlock_unlock(&mc_rwlock[node]);

			return mc;
		}
	}

	/* This is a cache miss */
	pthread_rwlock_unlock(&mc_rwlock[node]);
	return NULL;
}

void _starpu_memchunk_cache_insert(uint32_t node, starpu_mem_chunk_t mc)
{
	pthread_rwlock_wrlock(&mc_rwlock[node]);
	mc->data = NULL;
	starpu_mem_chunk_list_push_front(memchunk_cache[node], mc);
	pthread_rwlock_unlock(&mc_rwlock[node]);
}

/*
 * Free the memory chuncks that are explicitely tagged to be freed. The
 * mc_rwlock[node] rw-lock should be taken prior to calling this function.
 */
static size_t flush_memchunk_cache(uint32_t node)
{
	starpu_mem_chunk_t mc, next_mc;
	
	size_t freed = 0;

	for (mc = starpu_mem_chunk_list_begin(memchunk_cache[node]);
	     mc != starpu_mem_chunk_list_end(memchunk_cache[node]);
	     mc = next_mc)
	{
		next_mc = starpu_mem_chunk_list_next(mc);

		freed += free_memory_on_node(mc, node);

		starpu_mem_chunk_list_erase(memchunk_cache[node], mc);

		free(mc->interface);
		starpu_mem_chunk_delete(mc);
	}

	return freed;
}

/*
 * Try to free the buffers currently in use on the memory node. If the force
 * flag is set, the memory is freed regardless of coherency concerns (this
 * should only be used at the termination of StarPU for instance). The
 * mc_rwlock[node] rw-lock should be taken prior to calling this function.
 */
static size_t free_potentially_in_use_mc(uint32_t node, unsigned force)
{
	size_t freed = 0;

	starpu_mem_chunk_t mc, next_mc;

	for (mc = starpu_mem_chunk_list_begin(mc_list[node]);
	     mc != starpu_mem_chunk_list_end(mc_list[node]);
	     mc = next_mc)
	{
		/* there is a risk that the memory chunk is freed 
		   before next iteration starts: so we compute the next
		   element of the list now */
		next_mc = starpu_mem_chunk_list_next(mc);

		if (!force)
		{
			freed += try_to_free_mem_chunk(mc, node);
			#if 0
			if (freed > toreclaim)
				break;
			#endif
		}
		else {
			/* We must free the memory now: note that data
			 * coherency is not maintained in that case ! */
			freed += do_free_mem_chunk(mc, node);
		}
	}

	return freed;
}

/* 
 * Try to free some memory on the specified node
 * 	returns 0 if no memory was released, 1 else
 */

static size_t reclaim_memory(uint32_t node, size_t toreclaim __attribute__ ((unused)))
{
	int res;
	size_t freed = 0;

	res = pthread_rwlock_wrlock(&mc_rwlock[node]);
	STARPU_ASSERT(!res);

	/* remove all buffers for which there was a removal request */
	freed += flush_memchunk_cache(node);

	/* try to free all allocated data potentially in use */
	freed += free_potentially_in_use_mc(node, 0);

	res = pthread_rwlock_unlock(&mc_rwlock[node]);
	STARPU_ASSERT(!res);

	return freed;
}

/*
 * This function frees all the memory that was implicitely allocated by StarPU
 * (for the data replicates). This is not ensuring data coherency, and should
 * only be called while StarPU is getting shut down.
 */
size_t _starpu_free_all_automatically_allocated_buffers(uint32_t node)
{
	int res;

	size_t freed = 0;

	res = pthread_rwlock_wrlock(&mc_rwlock[node]);
	STARPU_ASSERT(!res);

	freed += flush_memchunk_cache(node);
	freed += free_potentially_in_use_mc(node, 1);

	res = pthread_rwlock_unlock(&mc_rwlock[node]);
	STARPU_ASSERT(!res);

	return freed;
}

starpu_mem_chunk_t _starpu_memchunk_init(starpu_data_handle handle, size_t size, void *interface, size_t interface_size, unsigned automatically_allocated)
{
	starpu_mem_chunk_t mc = starpu_mem_chunk_new();

	STARPU_ASSERT(handle);
	STARPU_ASSERT(handle->ops);

	mc->data = handle;
	mc->size = size;
	mc->footprint = _starpu_compute_data_footprint(handle);
	mc->ops = handle->ops;
	mc->data_was_deleted = 0;
	mc->automatically_allocated = automatically_allocated;

	/* Save a copy of the interface */
	mc->interface = malloc(interface_size);
	STARPU_ASSERT(mc->interface);
	memcpy(mc->interface, interface, interface_size);

	return mc;
}

static void register_mem_chunk(starpu_data_handle handle, uint32_t dst_node, size_t size, unsigned automatically_allocated)
{
	int res;

	starpu_mem_chunk_t mc;

	/* the interface was already filled by ops->allocate_data_on_node */
	void *src_interface = starpu_data_get_interface_on_node(handle, dst_node);
	size_t interface_size = handle->ops->interface_size;

	/* Put this memchunk in the list of memchunk in use */
	mc = _starpu_memchunk_init(handle, size, src_interface, interface_size, automatically_allocated); 

	res = pthread_rwlock_wrlock(&mc_rwlock[dst_node]);
	STARPU_ASSERT(!res);

	starpu_mem_chunk_list_push_front(mc_list[dst_node], mc);

	res = pthread_rwlock_unlock(&mc_rwlock[dst_node]);
	STARPU_ASSERT(!res);
}

/* This function is called when the handle is destroyed (eg. when calling
 * unregister or unpartition). */
void _starpu_request_mem_chunk_removal(starpu_data_handle handle, unsigned node)
{
	int res;
	res = pthread_rwlock_wrlock(&mc_rwlock[node]);
	STARPU_ASSERT(!res);

	/* iterate over the list of memory chunks and remove the entry */
	starpu_mem_chunk_t mc, next_mc;
	for (mc = starpu_mem_chunk_list_begin(mc_list[node]);
	     mc != starpu_mem_chunk_list_end(mc_list[node]);
	     mc = next_mc)
	{
		next_mc = starpu_mem_chunk_list_next(mc);

		if (mc->data == handle) {
			/* we found the data */
			mc->data_was_deleted = 1;

			/* remove it from the main list */
			starpu_mem_chunk_list_erase(mc_list[node], mc);

			/* put it in the list of buffers to be removed */
			starpu_mem_chunk_list_push_front(memchunk_cache[node], mc);

			res = pthread_rwlock_unlock(&mc_rwlock[node]);
			STARPU_ASSERT(!res);

			return;
		}
	}

	/* there was no corresponding buffer ... */
	res = pthread_rwlock_unlock(&mc_rwlock[node]);
	STARPU_ASSERT(!res);
}

static size_t free_memory_on_node(starpu_mem_chunk_t mc, uint32_t node)
{
	size_t freed = 0;

	STARPU_ASSERT(mc->ops);
	STARPU_ASSERT(mc->ops->free_data_on_node);

	starpu_data_handle handle = mc->data;

	/* Does this memory chunk refers to a handle that does not exist
	 * anymore ? */
	unsigned data_was_deleted = mc->data_was_deleted;

//	while (_starpu_spin_trylock(&handle->header_lock))
//		_starpu_datawizard_progress(_starpu_get_local_memory_node());

//FIXME: can we block here ?
//	_starpu_spin_lock(&handle->header_lock);

	if (mc->automatically_allocated && 
		(!handle || data_was_deleted || handle->per_node[node].refcnt == 0))
	{
		if (handle && !data_was_deleted)
			STARPU_ASSERT(handle->per_node[node].allocated);

		mc->ops->free_data_on_node(mc->interface, node);

		if (handle && !data_was_deleted)
		{
			handle->per_node[node].allocated = 0;

			/* XXX why do we need that ? */
			handle->per_node[node].automatically_allocated = 0;
		}

		freed = mc->size;

		if (handle && !data_was_deleted)
			STARPU_ASSERT(handle->per_node[node].refcnt == 0);
	}

//	_starpu_spin_unlock(&handle->header_lock);

	return freed;
}

/*
 * In order to allocate a piece of data, we try to reuse existing buffers if
 * its possible.
 *	1 - we try to reuse a memchunk that is explicitely unused.
 *	2 - we go through the list of memory chunks and find one that is not
 *	referenced and that has the same footprint to reuse it.
 *	3 - we call the usual driver's alloc method
 *	4 - we go through the list of memory chunks and release those that are
 *	not referenced (or part of those).
 *
 */

size_t _starpu_allocate_interface(starpu_data_handle handle, void *interface, uint32_t dst_node)
{
	unsigned attempts = 0;
	size_t allocated_memory;

	_starpu_data_allocation_inc_stats(dst_node);

#ifdef STARPU_USE_ALLOCATION_CACHE
	/* perhaps we can directly reuse a buffer in the free-list */
	uint32_t footprint = _starpu_compute_data_footprint(handle);

	STARPU_TRACE_START_ALLOC_REUSE(dst_node);
	if (try_to_find_reusable_mem_chunk(dst_node, handle, footprint))
	{
		_starpu_allocation_cache_hit(dst_node);
		return 0;
	}
	STARPU_TRACE_END_ALLOC_REUSE(dst_node);
#endif

	do {
		STARPU_ASSERT(handle->ops);
		STARPU_ASSERT(handle->ops->allocate_data_on_node);

		STARPU_TRACE_START_ALLOC(dst_node);
		allocated_memory = handle->ops->allocate_data_on_node(interface, dst_node);
		STARPU_TRACE_END_ALLOC(dst_node);

		if (!allocated_memory) {
			/* XXX perhaps we should find the proper granularity 
			 * not to waste our cache all the time */
			size_t data_size = _starpu_data_get_size(handle);

			STARPU_TRACE_START_MEMRECLAIM(dst_node);
			reclaim_memory(dst_node, 2*data_size);
			STARPU_TRACE_END_MEMRECLAIM(dst_node);
		}
		
	} while(!allocated_memory && attempts++ < 2);

	return allocated_memory;
}

int _starpu_allocate_memory_on_node(starpu_data_handle handle, uint32_t dst_node, unsigned may_alloc)
{
	size_t allocated_memory;

	STARPU_ASSERT(handle);

	/* A buffer is already allocated on the node */
	if (handle->per_node[dst_node].allocated)
		return 0;

	if (!may_alloc)
		return ENOMEM;

	void *interface = starpu_data_get_interface_on_node(handle, dst_node);
	allocated_memory = _starpu_allocate_interface(handle, interface, dst_node);

	/* perhaps we could really not handle that capacity misses */
	if (!allocated_memory)
		return ENOMEM;

	/* perhaps we could really not handle that capacity misses */
	if (allocated_memory)
		register_mem_chunk(handle, dst_node, allocated_memory, 1);

	handle->per_node[dst_node].allocated = 1;
	handle->per_node[dst_node].automatically_allocated = 1;

	return 0;
}

unsigned starpu_data_test_if_allocated_on_node(starpu_data_handle handle, uint32_t memory_node)
{
	return handle->per_node[memory_node].allocated;
}
