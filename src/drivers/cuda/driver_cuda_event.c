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

#include <cuda.h>
#include <cuda_runtime.h>

static int _starpu_cuda_event_init(starpu_event);
static int _starpu_cuda_event_wait(starpu_event);
static int _starpu_cuda_event_free(starpu_event);
static starpu_event_status_t _starpu_cuda_event_status(starpu_event);

static struct starpu_event_methods_t methods = {
   .init = _starpu_cuda_event_init,
   .wait = _starpu_cuda_event_wait,
   .free = _starpu_cuda_event_free,
   .status = _starpu_cuda_event_status
};

starpu_event _starpu_cuda_event_create(cudaEvent_t event) {
   return _starpu_event_create(&event, &methods, event);
}

static int _starpu_cuda_event_init(starpu_event event) {
   return 0;
}

static int _starpu_cuda_event_wait(starpu_event event) {
   cudaEvent_t ev = (cudaEvent_t)_starpu_event_data(event);
	cudaError_t err = cudaEventSynchronize(ev);

   if (STARPU_UNLIKELY(err))
   	STARPU_CUDA_REPORT_ERROR(err);

   return (err == cudaSuccess ? 0 : 1);
}

static int _starpu_cuda_event_free(starpu_event event) {
   cudaEvent_t ev = (cudaEvent_t)_starpu_event_data(event);
   cudaError_t err = cudaEventDestroy(ev);

   if (STARPU_UNLIKELY(err))
   	STARPU_CUDA_REPORT_ERROR(err);

   return (err == cudaSuccess ? 0 : 1);
}

static starpu_event_status_t _starpu_cuda_event_status(starpu_event event) {
   cl_event ev = (cudaEvent_t)_starpu_event_data(event);
   cudaError_t status;

   status = cudaEventQuery(ev);

   switch (status) {
      case cudaErrorInvalidValue:
      case cudaErrorNotReady:
         return STARPU_EVENT_WAITING;
      case cudaSuccess:
         return STARPU_EVENT_COMPLETE;
   }
}

#endif // STARPU_USE_CUDA
