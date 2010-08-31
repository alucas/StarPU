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

#include "dw_cholesky.h"
#include "dw_cholesky_models.h"

/* A [ y ] [ x ] */
float *A[NMAXBLOCKS][NMAXBLOCKS];
starpu_data_handle A_state[NMAXBLOCKS][NMAXBLOCKS];

/*
 *	Some useful functions
 */

static struct starpu_task *create_task()
{
	struct starpu_task *task = starpu_task_create();
		task->cl_arg = NULL;

	return task;
}

/*
 *	Create the codelets
 */

static starpu_codelet cl11 =
{
	.where = STARPU_CPU|STARPU_CUDA|STARPU_GORDON,
	.cpu_func = chol_cpu_codelet_update_u11,
#ifdef STARPU_USE_CUDA
	.cuda_func = chol_cublas_codelet_update_u11,
#endif
#ifdef STARPU_USE_GORDON
#ifdef SPU_FUNC_POTRF
	.gordon_func = SPU_FUNC_POTRF,
#else
#warning SPU_FUNC_POTRF is not available
#endif
#endif
	.nbuffers = 1,
	.model = &chol_model_11
};

static struct starpu_task * create_task_11(unsigned k, unsigned nblocks)
{
//	printf("task 11 k = %d TAG = %llx\n", k, (TAG11(k)));

	struct starpu_task *task = create_task(TAG11(k));
	
	task->cl = &cl11;

	/* which sub-data is manipulated ? */
	task->buffers[0].handle = A_state[k][k];
	task->buffers[0].mode = STARPU_RW;

	/* this is an important task */
	task->priority = STARPU_MAX_PRIO;

	return task;
}

static starpu_codelet cl21 =
{
	.where = STARPU_CPU|STARPU_CUDA|STARPU_GORDON,
	.cpu_func = chol_cpu_codelet_update_u21,
#ifdef STARPU_USE_CUDA
	.cuda_func = chol_cublas_codelet_update_u21,
#endif
#ifdef STARPU_USE_GORDON
#ifdef SPU_FUNC_STRSM
	.gordon_func = SPU_FUNC_STRSM,
#else
#warning SPU_FUNC_STRSM is not available
#endif
#endif
	.nbuffers = 2,
	.model = &chol_model_21
};

static struct starpu_task * create_task_21(unsigned k, unsigned j)
{
	struct starpu_task *task = create_task(TAG21(k, j));

	task->cl = &cl21;	

	/* which sub-data is manipulated ? */
	task->buffers[0].handle = A_state[k][k]; 
	task->buffers[0].mode = STARPU_R;
	task->buffers[1].handle = A_state[j][k]; 
	task->buffers[1].mode = STARPU_RW;

	if (j == k+1) {
		task->priority = STARPU_MAX_PRIO;
	}

	return task;
}

static starpu_codelet cl22 =
{
	.where = STARPU_CPU|STARPU_CUDA|STARPU_GORDON,
	.cpu_func = chol_cpu_codelet_update_u22,
#ifdef STARPU_USE_CUDA
	.cuda_func = chol_cublas_codelet_update_u22,
#endif
#ifdef STARPU_USE_GORDON
#ifdef SPU_FUNC_SGEMM
	.gordon_func = SPU_FUNC_SGEMM,
#else
#warning SPU_FUNC_SGEMM is not available
#endif
#endif
	.nbuffers = 3,
	.model = &chol_model_22
};

static struct starpu_task * create_task_22(unsigned k, unsigned i, unsigned j)
{
//	printf("task 22 k,i,j = %d,%d,%d TAG = %llx\n", k,i,j, TAG22(k,i,j));

	struct starpu_task *task = create_task(TAG22(k, i, j));

	task->cl = &cl22;

	/* which sub-data is manipulated ? */
	task->buffers[0].handle = A_state[i][k]; 
	task->buffers[0].mode = STARPU_R;
	task->buffers[1].handle = A_state[j][k]; 
	task->buffers[1].mode = STARPU_R;
	task->buffers[2].handle = A_state[j][i]; 
	task->buffers[2].mode = STARPU_RW;

	if ( (i == k + 1) && (j == k +1) ) {
		task->priority = STARPU_MAX_PRIO;
	}

	return task;
}



/*
 *	code to bootstrap the factorization 
 *	and construct the DAG
 */

static void dw_cholesky_no_stride(void)
{
	struct timeval start;
	struct timeval end;

	/* create all the DAG nodes */
	unsigned i,j,k;

   starpu_event first_event = starpu_event_create();
   starpu_event *events = malloc(sizeof(starpu_event) * nblocks *nblocks);
   #define EVENT(x,y) events[x*nblocks+y]
   EVENT(0,0) = first_event;
   starpu_event_retain(first_event);

	for (k = 0; k < nblocks; k++)
	{
		struct starpu_task *task11 = create_task_11(k, nblocks);
      starpu_event old = EVENT(k,k);
      starpu_task_submit_ex(task11, 1, &old, &EVENT(k,k));
      starpu_event_release(old);
		
		for (j = k+1; j<nblocks; j++)
		{
			struct starpu_task *task21 = create_task_21(k, j);
         if (k > 0) {
            starpu_event old = EVENT(k,j);
            starpu_event deps[] = {EVENT(k,k), old};
            starpu_task_submit_ex(task21, 2, deps, &EVENT(k,j));
            starpu_event_release(old);
         }
         else
            starpu_task_submit_ex(task21, 1, &EVENT(k,k), &EVENT(k,j));

			for (i = k+1; i<nblocks && i<=j; i++)
			{
            struct starpu_task *task22 = create_task_22(k, i, j);
            if (k > 0) {
               starpu_event old = EVENT(i,j);
               starpu_event deps[] = {EVENT(k,i), EVENT(k,j), old};
               starpu_task_submit_ex(task22, 3, deps, &EVENT(i,j));
               starpu_event_release(old);
            }
            else {
               starpu_event deps[] = {EVENT(k,i), EVENT(k,j)};
               starpu_task_submit_ex(task22, 2, deps, &EVENT(i,j));
            }
			}
		}
	}

   starpu_event last_event = EVENT(nblocks-1, nblocks-1);
   starpu_event_retain(last_event);

   for (i=0; i<nblocks; i++)
      for (j=0; j<nblocks; j++)
         starpu_event_release(EVENT(j,i));

   free(events);

	/* schedule the codelet */
	gettimeofday(&start, NULL);
   starpu_event_trigger(first_event);

	/* stall the application until the end of computations */
	starpu_event_wait(last_event);
	gettimeofday(&end, NULL);

   starpu_event_release(first_event);
   starpu_event_release(last_event);

	double timing = (double)((end.tv_sec - start.tv_sec)*1000000 + (end.tv_usec - start.tv_usec));
	fprintf(stderr, "Computation took (in ms)\n");
	printf("%2.2f\n", timing/1000);

	double flop = (1.0f*size*size*size)/3.0f;
	fprintf(stderr, "Synthetic GFlops : %2.2f\n", (flop/timing/1000.0f));
}

int main(int argc, char **argv)
{
	unsigned x, y;
	unsigned i, j;

	parse_args(argc, argv);
	assert(nblocks <= NMAXBLOCKS);

	fprintf(stderr, "BLOCK SIZE = %d\n", size / nblocks);

	starpu_init(NULL);

	/* Disable sequential consistency */
	starpu_data_set_default_sequential_consistency_flag(0);

	starpu_helper_cublas_init();

	for (y = 0; y < nblocks; y++)
	for (x = 0; x < nblocks; x++)
	{
		if (x <= y) {
			A[y][x] = malloc(BLOCKSIZE*BLOCKSIZE*sizeof(float));
			assert(A[y][x]);
		}
	}


	for (y = 0; y < nblocks; y++)
	for (x = 0; x < nblocks; x++)
	{
		if (x <= y) {
#ifdef STARPU_HAVE_POSIX_MEMALIGN
			posix_memalign((void **)&A[y][x], 128, BLOCKSIZE*BLOCKSIZE*sizeof(float));
#else
			A[y][x] = malloc(BLOCKSIZE*BLOCKSIZE*sizeof(float));
#endif
			assert(A[y][x]);
		}
	}

	/* create a simple definite positive symetric matrix example
	 *
	 *	Hilbert matrix : h(i,j) = 1/(i+j+1) ( + n In to make is stable ) 
	 * */
	for (y = 0; y < nblocks; y++)
	for (x = 0; x < nblocks; x++)
	if (x <= y) {
		for (i = 0; i < BLOCKSIZE; i++)
		for (j = 0; j < BLOCKSIZE; j++)
		{
			A[y][x][i*BLOCKSIZE + j] =
				(float)(1.0f/((float) (1.0+(x*BLOCKSIZE+i)+(y*BLOCKSIZE+j))));

			/* make it a little more numerically stable ... ;) */
			if ((x == y) && (i == j))
				A[y][x][i*BLOCKSIZE + j] += (float)(2*size);
		}
	}



	for (y = 0; y < nblocks; y++)
	for (x = 0; x < nblocks; x++)
	{
		if (x <= y) {
			starpu_matrix_data_register(&A_state[y][x], 0, (uintptr_t)A[y][x], 
				BLOCKSIZE, BLOCKSIZE, BLOCKSIZE, sizeof(float));
		}
	}

	dw_cholesky_no_stride();

	starpu_helper_cublas_shutdown();

	starpu_shutdown();
	return 0;
}


