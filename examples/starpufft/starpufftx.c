/*
 * StarPU
 * Copyright (C) Université Bordeaux 1, CNRS 2009 (see AUTHORS file)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR in PARTICULAR PURPOSE.
 *
 * See the GNU Lesser General Public License in COPYING.LGPL for more details.
 */

#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>

#include <starpu.h>
#include <config.h>

#include "starpufft.h"
#ifdef STARPU_USE_CUDA
#define _externC extern
#include "cudax_kernels.h"
#endif

#define _FFTW_FLAGS FFTW_ESTIMATE

enum steps {
	SPECIAL, TWIST1, FFT1, JOIN, TWIST2, FFT2, TWIST3, END
};

#define NUMBER_BITS 5
#define NUMBER_SHIFT (64 - NUMBER_BITS)
#define STEP_BITS 3
#define STEP_SHIFT (NUMBER_SHIFT - STEP_BITS)

#define I_BITS STEP_SHIFT

enum type {
	R2C,
	C2R,
	C2C
};

static unsigned task_per_worker[STARPU_NMAXWORKERS];
static unsigned samples_per_worker[STARPU_NMAXWORKERS];
static struct timeval start, submit_tasks, end;

/*
 *
 *	The actual kernels
 *
 */

struct STARPUFFT(plan) {
	int number;	/* uniquely identifies the plan, for starpu tags */

	int *n;
	int *n1;
	int *n2;
	int totsize;
	int totsize1;	/* Number of first-round tasks */
	int totsize2;	/* Size of first-round tasks */
	int totsize3;	/* Number of second-round tasks */
	int totsize4;	/* Size of second-round tasks */
	int dim;
	enum type type;
	int sign;

	STARPUFFT(complex) *roots[2];
	starpu_data_handle roots_handle[2];

	/* For each worker, we need some data */
	struct {
#ifdef STARPU_USE_CUDA
		/* CUFFT plans */
		cufftHandle plan1_cuda, plan2_cuda;
		/* Whether the plans above are initialized */
		int initialized1, initialized2;
#endif
#ifdef STARPU_HAVE_FFTW
		/* FFTW plans */
		_fftw_plan plan1_cpu, plan2_cpu;
		/* Buffers used by the plans above */
		_fftw_complex *in1, *out1;
		_fftw_complex *in2, *out2;
#endif
	} plans[STARPU_NMAXWORKERS];

	/* Buffers for codelets */
	STARPUFFT(complex) *in, *twisted1, *fft1, *twisted2, *fft2, *out;

	/* corresponding starpu DSM handles */
	starpu_data_handle in_handle, *twisted1_handle, *fft1_handle, *twisted2_handle, *fft2_handle;

	/* Tasks */
	struct starpu_task **twist1_tasks, **fft1_tasks, **twist2_tasks, **fft2_tasks, **twist3_tasks;
	struct starpu_task *join_task, *end_task;

	/* Arguments for tasks */
	struct STARPUFFT(args) *fft1_args, *fft2_args;
};

struct STARPUFFT(args) {
	struct STARPUFFT(plan) *plan;
	int i, j, jj, kk, ll, *iv, *kkv;
};

static void
check_dims(STARPUFFT(plan) plan)
{
	int dim;
	for (dim = 0; dim < plan->dim; dim++)
		if (plan->n[dim] & (plan->n[dim]-1)) {
			fprintf(stderr,"can't cope with non-power-of-2\n");
			STARPU_ABORT();
		}
}

static void
compute_roots(STARPUFFT(plan) plan)
{
	int dim, k;

	/* Compute the n-roots and m-roots of unity for twiddling */
	for (dim = 0; dim < plan->dim; dim++) {
		STARPUFFT(complex) exp = (plan->sign * 2. * 4.*atan(1.)) * _Complex_I / (STARPUFFT(complex)) plan->n[dim];
		plan->roots[dim] = malloc(plan->n[dim] * sizeof(**plan->roots));
		for (k = 0; k < plan->n[dim]; k++)
			plan->roots[dim][k] = cexp(exp*k);
		starpu_vector_data_register(&plan->roots_handle[dim], 0, (uintptr_t) plan->roots[dim], plan->n[dim], sizeof(**plan->roots));

#ifdef STARPU_USE_CUDA
		if (plan->n[dim] > 100000) {
			/* prefetch the big root array on GPUs */
			unsigned worker;
			unsigned nworkers = starpu_worker_get_count();
			for (worker = 0; worker < nworkers; worker++)
			{
				unsigned node = starpu_worker_get_memory_node(worker);
				if (starpu_worker_get_type(worker) == STARPU_CUDA_WORKER)
					starpu_data_prefetch_on_node(plan->roots_handle[dim], node, 0);
			}
		}
#endif
	}
}

#include "starpufftx1d.c"
#include "starpufftx2d.c"

starpu_event
STARPUFFT(start)(STARPUFFT(plan) plan, void *_in, void *_out)
{
	starpu_event event;
	int z;

	plan->in = _in;
	plan->out = _out;

	switch (plan->dim) {
		case 1: {
			switch (plan->type) {
			case C2C:
				starpu_vector_data_register(&plan->in_handle, 0, (uintptr_t) plan->in, plan->totsize, sizeof(STARPUFFT(complex)));
				for (z = 0; z < plan->totsize1; z++)
					plan->twist1_tasks[z]->buffers[0].handle = plan->in_handle;
				event = STARPUFFT(start1dC2C)(plan);
				break;
			default:
				STARPU_ABORT();
				break;
			}
			break;
		}
		case 2:
			starpu_vector_data_register(&plan->in_handle, 0, (uintptr_t) plan->in, plan->totsize, sizeof(STARPUFFT(complex)));
			for (z = 0; z < plan->totsize1; z++)
				plan->twist1_tasks[z]->buffers[0].handle = plan->in_handle;
			event = STARPUFFT(start2dC2C)(plan);
			break;
		default:
			STARPU_ABORT();
			break;
	}
	return event;
}

void
STARPUFFT(cleanup)(STARPUFFT(plan) plan)
{
	starpu_data_unregister(plan->in_handle);
}

void
STARPUFFT(execute)(STARPUFFT(plan) plan, void *in, void *out)
{
	memset(task_per_worker, 0, sizeof(task_per_worker));
	memset(samples_per_worker, 0, sizeof(task_per_worker));

	gettimeofday(&start, NULL);

	starpu_event event = STARPUFFT(start)(plan, in, out);
	gettimeofday(&submit_tasks, NULL);
	starpu_event_wait(event);
   starpu_event_release(event);

	STARPUFFT(cleanup)(plan);

	gettimeofday(&end, NULL);
}

/* Destroy FFTW plans, unregister and free buffers, and free tags */
void
STARPUFFT(destroy_plan)(STARPUFFT(plan) plan)
{
	unsigned workerid;
   int i, dim;

	for (workerid = 0; workerid < starpu_worker_get_count(); workerid++) {
		switch (starpu_worker_get_type(workerid)) {
		case STARPU_CPU_WORKER:
#ifdef STARPU_HAVE_FFTW
			_FFTW(free)(plan->plans[workerid].in1);
			_FFTW(free)(plan->plans[workerid].out1);
			_FFTW(destroy_plan)(plan->plans[workerid].plan1_cpu);
			_FFTW(free)(plan->plans[workerid].in2);
			_FFTW(free)(plan->plans[workerid].out2);
			_FFTW(destroy_plan)(plan->plans[workerid].plan2_cpu);
#endif
			break;
		case STARPU_CUDA_WORKER:
#ifdef STARPU_USE_CUDA
			/* FIXME: Can't deallocate */
#endif
			break;
		default:
			STARPU_ABORT();
			break;
		}
	}
	for (i = 0; i < plan->totsize1; i++) {
		starpu_data_unregister(plan->twisted1_handle[i]);
		free(plan->twist1_tasks[i]);
		starpu_data_unregister(plan->fft1_handle[i]);
		free(plan->fft1_tasks[i]);
	}

	free(plan->twisted1_handle);
	free(plan->twist1_tasks);
	free(plan->fft1_handle);
	free(plan->fft1_tasks);
	free(plan->fft1_args);

	free(plan->join_task);

	for (i = 0; i < plan->totsize3; i++) {
		starpu_data_unregister(plan->twisted2_handle[i]);
		free(plan->twist2_tasks[i]);
		starpu_data_unregister(plan->fft2_handle[i]);
		free(plan->fft2_tasks[i]);
		free(plan->twist3_tasks[i]);
	}

	free(plan->end_task);

	free(plan->twisted2_handle);
	free(plan->twist2_tasks);
	free(plan->fft2_handle);
	free(plan->fft2_tasks);
	free(plan->twist3_tasks);
	free(plan->fft2_args);

	for (dim = 0; dim < plan->dim; dim++) {
		starpu_data_unregister(plan->roots_handle[dim]);
		free(plan->roots[dim]);
	}

	free(plan->n);
	free(plan->n1);
	free(plan->n2);
	STARPUFFT(free)(plan->twisted1);
	STARPUFFT(free)(plan->fft1);
	STARPUFFT(free)(plan->twisted2);
	STARPUFFT(free)(plan->fft2);
	free(plan);
}

void *
STARPUFFT(malloc)(size_t n)
{
#ifdef STARPU_USE_CUDA
	void *res;
	starpu_data_malloc_pinned_if_possible(&res, n);
	return res;
#else
#  ifdef STARPU_HAVE_FFTW
	return _FFTW(malloc)(n);
#  else
	return malloc(n);
#  endif
#endif
}

void
STARPUFFT(free)(void *p)
{
#ifdef STARPU_USE_CUDA
	// TODO: FIXME
#else
#  ifdef STARPU_HAVE_FFTW
	_FFTW(free)(p);
#  else
	free(p);
#  endif
#endif
}

void
STARPUFFT(showstats)(FILE *out)
{
	unsigned worker;
	unsigned total;

#define TIMING(begin,end) (double)((end.tv_sec - begin.tv_sec)*1000000 + (end.tv_usec - begin.tv_usec))
#define MSTIMING(begin,end) (TIMING(begin,end)/1000.)
	double paratiming = TIMING(start,end);
	fprintf(out, "Tasks submission took %2.2f ms\n", MSTIMING(start,submit_tasks));
	fprintf(out, "Tasks termination took %2.2f ms\n", MSTIMING(submit_tasks,end));

	fprintf(out, "Total %2.2f ms\n", MSTIMING(start,end));

	for (worker = 0, total = 0; worker < starpu_worker_get_count(); worker++)
		total += task_per_worker[worker];

	for (worker = 0; worker < starpu_worker_get_count(); worker++)
	{
		if (task_per_worker[worker])
		{
			char name[32];
			starpu_worker_get_name(worker, name, 32);

			unsigned long bytes = sizeof(STARPUFFT(complex))*samples_per_worker[worker];

			fprintf(stderr, "\t%s -> %2.2f MB\t%2.2f\tMB/s\t%u %2.2f %%\n", name, (1.0*bytes)/(1024*1024), bytes/paratiming, task_per_worker[worker], (100.0*task_per_worker[worker])/total);
		}
	}
}
