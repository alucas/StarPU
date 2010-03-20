/*
 * StarPU
 * Copyright (C) INRIA 2008-2010 (see AUTHORS file)
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

#include "SobolQRNG/sobol.h"
#include "pi.h"

#ifdef STARPU_USE_CUDA
void cuda_kernel(void **descr, void *cl_arg);
#endif

static void cpu_kernel(void *descr[], void *cl_arg)
{
	unsigned *directions = (unsigned *)STARPU_GET_VECTOR_PTR(descr[0]);
	unsigned nx = NSHOT_PER_TASK;

	TYPE *random_numbers = malloc(2*nx*sizeof(TYPE));
	sobolCPU(2*nx/n_dimensions, n_dimensions, directions, random_numbers);

	TYPE *random_numbers_x = &random_numbers[0];
	TYPE *random_numbers_y = &random_numbers[nx];

	unsigned current_cnt = 0;

	unsigned i;
	for (i = 0; i < nx; i++)
	{
		TYPE x = random_numbers_x[i];
		TYPE y = random_numbers_y[i];

		TYPE dist = (x*x + y*y);

		unsigned success = (dist <= 1.0);
		current_cnt += success;
	}

	unsigned *cnt = (unsigned *)STARPU_GET_VECTOR_PTR(descr[1]);
	*cnt = current_cnt;

	free(random_numbers);
}



int main(int argc, char **argv)
{
	unsigned i;

	starpu_init(NULL);

	/* Initialize the random number generator */
	unsigned *sobol_qrng_directions = malloc(n_dimensions*n_directions*sizeof(unsigned));
	STARPU_ASSERT(sobol_qrng_directions);

	initSobolDirectionVectors(n_dimensions, sobol_qrng_directions);

	/* Any worker may use that array now */
	starpu_data_handle sobol_qrng_direction_handle;
	starpu_register_vector_data(&sobol_qrng_direction_handle, 0,
		(uintptr_t)sobol_qrng_directions, n_dimensions*n_directions, sizeof(unsigned));

	unsigned *cnt_array = malloc(NTASKS*sizeof(unsigned));
	STARPU_ASSERT(cnt_array);
	starpu_data_handle cnt_array_handle;
	starpu_register_vector_data(&cnt_array_handle, 0, (uintptr_t)cnt_array, NTASKS, sizeof(unsigned));

	/* TODO use a write-back mechanism */

	struct starpu_filter_t f = {
		.filter_func = starpu_block_filter_func_vector,
		.filter_arg = NTASKS
	};
	
#if 0
	starpu_partition_data(random_array_handle_x, &f);
	starpu_partition_data(random_array_handle_y, &f);
#endif
	starpu_partition_data(cnt_array_handle, &f);

	static struct starpu_perfmodel_t model = {
		.type = STARPU_HISTORY_BASED,
		.symbol = "monte_carlo_pi"
	};

	struct starpu_codelet_t cl = {
		.where = STARPU_CPU|STARPU_CUDA,
		.cpu_func = cpu_kernel,
#ifdef STARPU_USE_CUDA
		.cuda_func = cuda_kernel,
#endif
		.nbuffers = 2,
		.model = &model
	};

	for (i = 0; i < NTASKS; i++)
	{
		struct starpu_task *task = starpu_task_create();

		task->cl = &cl;

		STARPU_ASSERT(starpu_get_sub_data(cnt_array_handle, 1, i));

		task->buffers[0].handle = sobol_qrng_direction_handle;
		task->buffers[0].mode   = STARPU_R;
		task->buffers[1].handle = starpu_get_sub_data(cnt_array_handle, 1, i);
		task->buffers[1].mode   = STARPU_W;

		int ret = starpu_submit_task(task);
		STARPU_ASSERT(!ret);
	}

	starpu_wait_all_tasks();

	/* Get the cnt_array back in main memory */
	starpu_unpartition_data(cnt_array_handle, 0);
	starpu_sync_data_with_mem(cnt_array_handle, STARPU_RW);

	/* Count the total number of entries */
	unsigned long total_cnt = 0;
	for (i = 0; i < NTASKS; i++)
		total_cnt += cnt_array[i];

	starpu_release_data_from_mem(cnt_array_handle);

	starpu_shutdown();

	/* Total surface : Pi * r^ 2 = Pi*1^2, total square surface : 2^2 = 4, probability to impact the disk: pi/4 */

	fprintf(stderr, "Pi approximation : %f (%ld / %ld)\n", ((TYPE)total_cnt*4)/(SIZE), total_cnt, SIZE);

	return 0;
}
