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

#include <stdio.h>
#include <starpu.h>

#define NITER	10
#define SIZE	(4*1024*1024*sizeof(float))

static float *data = NULL;

int main(int argc, char **argv)
{
	starpu_init(NULL);

	unsigned iter;
	for (iter = 0; iter < NITER; iter++)
	{
		starpu_data_malloc_pinned_if_possible((void **)&data, SIZE);
		starpu_data_free_pinned_if_possible(data);
	}

	starpu_shutdown();

	return 0;
}
