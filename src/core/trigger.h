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

#ifndef __TRIGGER_H__
#define __TRIGGER_H__

typedef struct starpu_trigger_t * starpu_trigger;

void _starpu_trigger_init(starpu_trigger trigger, void (*callback)(starpu_trigger, void*), void *data);

void _starpu_trigger_signal(starpu_trigger trigger);

#endif // __TRIGGER_H__
