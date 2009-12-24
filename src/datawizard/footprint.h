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

#ifndef __FOOTPRINT_H__
#define __FOOTPRINT_H__

#include <core/jobs.h>

struct job_s;

void compute_buffers_footprint(struct job_s *j);
inline uint32_t compute_data_footprint(starpu_data_handle handle);

#endif // __FOOTPRINT_H__
