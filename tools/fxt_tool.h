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

#ifndef __FXT_TOOL_H__
#define __FXT_TOOL_H__

#include <search.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <common/fxt.h>
#include <common/list.h>
#include <starpu_mpi_fxt.h>
#include <starpu.h>

#include "histo_paje.h"

#define FACTOR  100

extern void init_dag_dot(void);
extern void terminate_dat_dot(void);
extern void add_deps(uint64_t child, uint64_t father);
extern void dot_set_tag_done(uint64_t tag, const char *color);
extern void dot_set_task_done(unsigned long job_id, const char *label, const char *color);

void set_next_other_worker_color(int workerid);
void set_next_cpu_worker_color(int workerid);
void set_next_cuda_worker_color(int workerid);
const char *get_worker_color(int workerid);

unsigned get_colour_symbol_red(char *name);
unsigned get_colour_symbol_green(char *name);
unsigned get_colour_symbol_blue(char *name);

void reinit_colors(void);

/*
 *	MPI
 */

int find_sync_point(char *filename_in, uint64_t *offset, int *key, int *rank);
uint64_t find_start_time(char *filename_in);

struct mpi_transfer {
	unsigned matched;
	int other_rank; /* src for a recv, dest for a send */
	int mpi_tag;
	size_t size;
	float date;
};

void add_mpi_send_transfer(int src, int dst, int mpi_tag, size_t size, float date);
void add_mpi_recv_transfer(int src, int dst, int mpi_tag, float date);

#endif // __FXT_TOOL_H__
