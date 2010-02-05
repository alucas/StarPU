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

#ifndef __STARPU_EXPERT_H__
#define __STARPU_EXPERT_H__

#include <starpu.h>
#include <starpu_config.h>

#ifdef __cplusplus
extern "C" {
#endif

void starpu_wake_all_blocked_workers(void);

int starpu_register_progression_hook(unsigned (*func)(void *arg), void *arg);
void starpu_deregister_progression_hook(int hook_id);

#ifdef __cplusplus
}
#endif

#endif // __STARPU_H__