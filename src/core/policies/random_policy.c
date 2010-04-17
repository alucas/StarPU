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

#include <core/policies/random_policy.h>

/* XXX 32 is set randomly */
static unsigned nworkers;
static struct starpu_jobq_s *queue_array[32];

static starpu_job_t random_pop_task(struct starpu_jobq_s *q)
{
	struct starpu_job_s *j;

	j = _starpu_fifo_pop_task(q);

	return j;
}

static int _random_push_task(struct starpu_jobq_s *q __attribute__ ((unused)), starpu_job_t task, unsigned prio)
{
	/* find the queue */
	struct starpu_fifo_jobq_s *fifo;
	unsigned worker;

	unsigned selected = 0;

	double alpha_sum = 0.0;

	for (worker = 0; worker < nworkers; worker++)
	{
		alpha_sum += queue_array[worker]->alpha;
	}

	double random = starpu_drand48()*alpha_sum;
//	fprintf(stderr, "my rand is %e\n", random);

	double alpha = 0.0;
	for (worker = 0; worker < nworkers; worker++)
	{
		if (alpha + queue_array[worker]->alpha > random) {
			/* we found the worker */
			selected = worker;
			break;
		}

		alpha += queue_array[worker]->alpha;
	}

	/* we should now have the best worker in variable "best" */
	fifo = queue_array[selected]->queue;

	if (prio) {
		return _starpu_fifo_push_prio_task(queue_array[selected], task);
	} else {
		return _starpu_fifo_push_task(queue_array[selected], task);
	}
}

static int random_push_prio_task(struct starpu_jobq_s *q, starpu_job_t task)
{
	return _random_push_task(q, task, 1);
}

static int random_push_task(struct starpu_jobq_s *q, starpu_job_t task)
{
	return _random_push_task(q, task, 0);
}

static struct starpu_jobq_s *init_random_fifo(void)
{
	struct starpu_jobq_s *q;

	q = _starpu_create_fifo();

	q->_starpu_push_task = random_push_task; 
	q->push_prio_task = random_push_prio_task; 
	q->_starpu_pop_task = random_pop_task;
	q->who = 0;

	queue_array[nworkers++] = q;

	return q;
}

static void initialize_random_policy(struct starpu_machine_config_s *config, 
	 __attribute__ ((unused)) struct starpu_sched_policy_s *_policy) 
{
	nworkers = 0;

	starpu_srand48(time(NULL));

	_starpu_setup_queues(_starpu_init_fifo_queues_mechanisms, init_random_fifo, config);
}

static struct starpu_jobq_s *get_local_queue_random(struct starpu_sched_policy_s *policy __attribute__ ((unused)))
{
	struct starpu_jobq_s *queue;
	queue = pthread_getspecific(policy->local_queue_key);

	if (!queue)
	{
		/* take one randomly as this *must* be for a push anyway XXX */
		queue = queue_array[0];
	}

	return queue;
}

struct starpu_sched_policy_s _starpu_sched_random_policy = {
	.init_sched = initialize_random_policy,
	.deinit_sched = NULL,
	.starpu_get_local_queue = get_local_queue_random,
	.policy_name = "random",
	.policy_description = "weighted random"
};