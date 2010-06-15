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

#ifndef __DECLS_H__
#define __DECLS_H__

#include <common/list.h>

#define PURE __attribute((pure))

typedef struct starpu_data_chunk_t * starpu_data_chunk;

LIST_DECLARE_TYPE(starpu_job);
typedef struct starpu_jobq_s *starpu_job_queue;

LIST_DECLARE_TYPE(starpu_data_request);
#endif // __DECLS_H__
