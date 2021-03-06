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

#include <pthread.h>

#include <starpu.h>
#include <common/config.h>
#include <common/utils.h>
#include <core/sched_policy.h>

static struct starpu_sched_policy_s policy;

static int use_prefetch = 0;

int _starpu_get_prefetch_flag(void)
{
	return use_prefetch;
}

/*
 *	Predefined policies
 */

extern struct starpu_sched_policy_s _starpu_sched_ws_policy;
extern struct starpu_sched_policy_s _starpu_sched_prio_policy;
extern struct starpu_sched_policy_s _starpu_sched_no_prio_policy;
extern struct starpu_sched_policy_s _starpu_sched_random_policy;
extern struct starpu_sched_policy_s _starpu_sched_dm_policy;
extern struct starpu_sched_policy_s _starpu_sched_dmda_policy;
extern struct starpu_sched_policy_s _starpu_sched_dmda_ready_policy;
extern struct starpu_sched_policy_s _starpu_sched_dmda_sorted_policy;
extern struct starpu_sched_policy_s _starpu_sched_eager_policy;

#define NPREDEFINED_POLICIES	9

static struct starpu_sched_policy_s *predefined_policies[NPREDEFINED_POLICIES] = {
	&_starpu_sched_ws_policy,
	&_starpu_sched_prio_policy,
	&_starpu_sched_no_prio_policy,
	&_starpu_sched_dm_policy,
	&_starpu_sched_dmda_policy,
	&_starpu_sched_dmda_ready_policy,
	&_starpu_sched_dmda_sorted_policy,
	&_starpu_sched_random_policy,
	&_starpu_sched_eager_policy
};

struct starpu_sched_policy_s *_starpu_get_sched_policy(void)
{
	return &policy;
}

/*
 *	Methods to initialize the scheduling policy
 */

static void load_sched_policy(struct starpu_sched_policy_s *sched_policy)
{
	STARPU_ASSERT(sched_policy);

#ifdef STARPU_VERBOSE
	if (sched_policy->policy_name)
	{
		if (sched_policy->policy_description)
                        _STARPU_DEBUG("Use %s scheduler (%s)\n", sched_policy->policy_name, sched_policy->policy_description);
                else
                        _STARPU_DEBUG("Use %s scheduler \n", sched_policy->policy_name);

	}
#endif

	policy.init_sched = sched_policy->init_sched;
	policy.deinit_sched = sched_policy->deinit_sched;
	policy.push_task = sched_policy->push_task;
	policy.push_prio_task = sched_policy->push_prio_task;
	policy.pop_task = sched_policy->pop_task;
        policy.post_exec_hook = sched_policy->post_exec_hook;
	policy.pop_every_task = sched_policy->pop_every_task;
}

static struct starpu_sched_policy_s *find_sched_policy_from_name(const char *policy_name)
{

	if (!policy_name)
		return NULL;

	unsigned i;
	for (i = 0; i < NPREDEFINED_POLICIES; i++)
	{
		struct starpu_sched_policy_s *p;
		p = predefined_policies[i];
		if (p->policy_name)
		{
			if (strcmp(policy_name, p->policy_name) == 0) {
				/* we found a policy with the requested name */
				return p;
			}
		}
	}

	/* nothing was found */
	return NULL;
}

static void display_sched_help_message(void)
{
	const char *sched_env = getenv("STARPU_SCHED");
	if (sched_env && (strcmp(sched_env, "help") == 0)) {
		fprintf(stderr, "STARPU_SCHED can be either of\n");

		/* display the description of all predefined policies */
		unsigned i;
		for (i = 0; i < NPREDEFINED_POLICIES; i++)
		{
			struct starpu_sched_policy_s *p;
			p = predefined_policies[i];
			fprintf(stderr, "%s\t-> %s\n", p->policy_name, p->policy_description);
		}
	 }
}

static struct starpu_sched_policy_s *select_sched_policy(struct starpu_machine_config_s *config)
{
	struct starpu_sched_policy_s *selected_policy = NULL;
	struct starpu_conf *user_conf = config->user_conf;

	/* First, we check whether the application explicitely gave a scheduling policy or not */
	if (user_conf && (user_conf->sched_policy))
		return user_conf->sched_policy;

	/* Otherwise, we look if the application specified the name of a policy to load */
	const char *sched_pol_name;
	if (user_conf && (user_conf->sched_policy_name))
	{
		sched_pol_name = user_conf->sched_policy_name;
	}
	else {
		sched_pol_name = getenv("STARPU_SCHED");
	}

	if (sched_pol_name)
		selected_policy = find_sched_policy_from_name(sched_pol_name);

	/* Perhaps there was no policy that matched the name */
	if (selected_policy)
		return selected_policy;

	/* If no policy was specified, we use the greedy policy as a default */
	return &_starpu_sched_eager_policy;
}

void _starpu_init_sched_policy(struct starpu_machine_config_s *config)
{
	/* Perhaps we have to display some help */
	display_sched_help_message();

	/* Prefetch is activated by default */
	use_prefetch = starpu_get_env_number("STARPU_PREFETCH");
	if (use_prefetch == -1)
		use_prefetch = 1;

	/* By default, we don't calibrate */
	unsigned do_calibrate = 0;
	if (config->user_conf && (config->user_conf->calibrate != -1))
	{
		do_calibrate = config->user_conf->calibrate;
	}
	else {
		int res = starpu_get_env_number("STARPU_CALIBRATE");
		do_calibrate =  (res < 0)?0:(unsigned)res;
	}

	_starpu_set_calibrate_flag(do_calibrate);

	struct starpu_sched_policy_s *selected_policy;
	selected_policy = select_sched_policy(config);

	load_sched_policy(selected_policy);

	policy.init_sched(&config->topology, &policy);
}

void _starpu_deinit_sched_policy(struct starpu_machine_config_s *config)
{
	if (policy.deinit_sched)
		policy.deinit_sched(&config->topology, &policy);
}

/* the generic interface that call the proper underlying implementation */
int _starpu_push_task(starpu_job_t j, unsigned job_is_already_locked)
{
	struct starpu_task *task = j->task;

	task->status = STARPU_TASK_READY;

	/* in case there is no codelet associated to the task (that's a control
	 * task), we directly execute its callback and enforce the
	 * corresponding dependencies */
	if (task->cl == NULL)
	{
		_starpu_handle_job_termination(j, job_is_already_locked);
		return 0;
	}

	if (STARPU_UNLIKELY(task->execute_on_a_specific_worker))
	{
		unsigned workerid = task->workerid;
		struct starpu_worker_s *worker = _starpu_get_worker_struct(workerid);
		
		if (use_prefetch)
		{
			uint32_t memory_node = starpu_worker_get_memory_node(workerid); 
			_starpu_prefetch_task_input_on_node(task, memory_node);
		}

		return _starpu_push_local_task(worker, j);
	}
	else {
		STARPU_ASSERT(policy.push_task);

		return policy.push_task(task);
	}
}

struct starpu_task *_starpu_pop_task(void)
{
	return policy.pop_task();
}

/* pop every task that can be executed on "where" (eg. GORDON) */
struct starpu_task *_starpu_pop_every_task(uint32_t where)
{
	STARPU_ASSERT(policy.pop_every_task);

	return policy.pop_every_task(where);
}

void _starpu_sched_post_exec_hook(struct starpu_task *task)
{
	if (policy.post_exec_hook)
		policy.post_exec_hook(task);
}

void _starpu_wait_on_sched_event(void)
{
	struct starpu_worker_s *worker = _starpu_get_local_worker_key();

	PTHREAD_MUTEX_LOCK(worker->sched_mutex);

	_starpu_handle_all_pending_node_data_requests(worker->memory_node);

	if (_starpu_machine_is_running())
	{
#ifndef STARPU_NON_BLOCKING_DRIVERS
		pthread_cond_wait(worker->sched_cond, worker->sched_mutex);
#endif
	}

	PTHREAD_MUTEX_UNLOCK(worker->sched_mutex);
}
