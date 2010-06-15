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

#include <common/config.h>
#include <common/utils.h>
#include <common/decls.h>
#include <starpu.h>

#ifdef __MINGW32__
#include <windows.h>
#endif

static enum { UNINITIALIZED, CHANGING, INITIALIZED } initialized = UNINITIALIZED;

int starpu_init(struct starpu_conf *user_conf)
{
	int ret;

	PTHREAD_MUTEX_LOCK(&init_mutex);
	while (initialized == CHANGING)
		/* Wait for the other one changing it */
		PTHREAD_COND_WAIT(&init_cond, &init_mutex);
	init_count++;
	if (initialized == INITIALIZED)
		/* He initialized it, don't do it again */
		return 0;
	/* initialized == UNINITIALIZED */
	initialized = CHANGING;
	PTHREAD_MUTEX_UNLOCK(&init_mutex);

#ifdef __MINGW32__
	WSADATA wsadata;
	WSAStartup(MAKEWORD(1,0), &wsadata);
#endif

	srand(2008);
	
#ifdef STARPU_USE_FXT
	_starpu_start_fxt_profiling();
#endif
	
	_starpu_open_debug_logfile();

	_starpu_timing_init();

	_starpu_load_bus_performance_files();

	/* store the pointer to the user explicit configuration during the
	 * initialization */
	config.user_conf = user_conf;

	ret = _starpu_build_topology(&config);
	if (ret) {
		PTHREAD_MUTEX_LOCK(&init_mutex);
		init_count--;
		initialized = UNINITIALIZED;
		/* Let somebody else try to do it */
		PTHREAD_COND_SIGNAL(&init_cond);
		PTHREAD_MUTEX_UNLOCK(&init_mutex);
		return ret;
	}

	/* We need to store the current task handled by the different
	 * threads */
	_starpu_initialize_current_task_key();	

	/* initialize the scheduler */

	/* initialize the queue containing the jobs */
	_starpu_init_sched_policy(&config);

	_starpu_initialize_registered_performance_models();

	_starpu_init_workers(&config);

	PTHREAD_MUTEX_LOCK(&init_mutex);
	initialized = INITIALIZED;
	/* Tell everybody that we initialized */
	PTHREAD_COND_BROADCAST(&init_cond);
	PTHREAD_MUTEX_UNLOCK(&init_mutex);

	return 0;
}
