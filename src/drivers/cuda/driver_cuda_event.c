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

#ifdef STARPU_USE_CUDA

#include <core/event.h>
#include <cuda.h>
#include <cuda_runtime.h>

struct arg_t {
   cudaEvent_t cuda_event;
   starpu_event event;
};

static void * cuda_event_thread(void *args) {
   struct arg_t *arg = (struct arg_t*)args;

	cudaError_t err = cudaEventSynchronize(arg->cuda_event);

   if (STARPU_UNLIKELY(err))
   	STARPU_CUDA_REPORT_ERROR(err);

   _starpu_event_complete(arg->event);

   err = cudaEventDestroy(arg->cuda_event);

   if (STARPU_UNLIKELY(err))
   	STARPU_CUDA_REPORT_ERROR(err);

   free(arg);
}

starpu_event _starpu_cuda_event_create(cudaEvent_t event) {
   struct arg_t * arg = malloc(sizeof(struct arg_t));

   arg->event      = _starpu_event_create();
   arg->cuda_event = event;

   pthread_t thread;
   pthread_create(&thread, NULL, cuda_event_thread, arg);
   pthread_detach(thread);

   return arg->event;
}

#endif // STARPU_USE_CUDA
