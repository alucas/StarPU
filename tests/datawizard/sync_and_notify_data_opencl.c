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

#include <starpu.h>
#include <starpu_opencl.h>
#include <CL/cl.h>

extern struct starpu_opencl_program opencl_code;

void opencl_codelet_incA(void *descr[], __attribute__ ((unused)) void *_args)
{
        unsigned *val = (unsigned *)STARPU_VECTOR_GET_PTR(descr[0]);
	cl_kernel kernel;
	cl_command_queue queue;
	int id, devid, err;

	id = starpu_worker_get_id();
	devid = starpu_worker_get_devid(id);

	err = starpu_opencl_load_kernel(&kernel, &queue, &opencl_code, "incA", devid);
	if (err != CL_SUCCESS) STARPU_OPENCL_REPORT_ERROR(err);

	err = 0;
	err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &val);
	if (err) STARPU_OPENCL_REPORT_ERROR(err);

	{
		size_t global=100;
		size_t local=100;
		err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global, &local, 0, NULL, NULL);
		if (err != CL_SUCCESS) STARPU_OPENCL_REPORT_ERROR(err);
	}

	clFinish(queue);

	starpu_opencl_release_kernel(kernel);
}

void opencl_codelet_incC(void *descr[], __attribute__ ((unused)) void *_args)
{
	unsigned *val = (unsigned *)STARPU_VECTOR_GET_PTR(descr[0]);
	cl_kernel kernel;
	cl_command_queue queue;
	int id, devid, err;

	id = starpu_worker_get_id();
	devid = starpu_worker_get_devid(id);

	err = starpu_opencl_load_kernel(&kernel, &queue, &opencl_code, "incC", devid);
	if (err != CL_SUCCESS) STARPU_OPENCL_REPORT_ERROR(err);

	err = 0;
	err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &val);
	if (err) STARPU_OPENCL_REPORT_ERROR(err);

	{
		size_t global=100;
		size_t local=100;
		err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global, &local, 0, NULL, NULL);
		if (err != CL_SUCCESS) STARPU_OPENCL_REPORT_ERROR(err);
	}

	clFinish(queue);

	starpu_opencl_release_kernel(kernel);
}
