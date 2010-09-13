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

#include <drivers/drivers.h>
#include <drivers/common/common.h>

/* This is the only place where we #ifdef for drivers */
static starpu_driver drivers[] {
   #ifdef STARPU_USE_CPU
   &_starpu_driver_cpu,
   #endif

   #ifdef STARPU_USE_OPENCL
   &_starpu_driver_opencl,
   #endif

   #ifdef STARPU_USE_CUDA
   &_starpu_driver_cuda,
   #endif
}

static unsigned num_drivers = sizeof(drivers) / sizeof(starpu_driver);


int _starpu_drivers_init(unsigned *n, starpu_driver*d) {

   *n = num_drivers;
   *d = &drivers;

   return 0;
}

int _starpu_drivers_release() {
   unsigned i;
   for (i=0; i<num_drivers; i++)
      starpu_driver_release(drivers[i]);

   num_drivers = 0;

   return 0;
}
