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

#ifndef __PERFMODEL_H__
#define __PERFMODEL_H__

#include <common/config.h>
#include <starpu.h>
#include <starpu_perfmodel.h>
//#include <core/jobs.h>
#include <common/htable32.h>
//#include <core/workers.h>
#include <pthread.h>
#include <stdio.h>

struct starpu_buffer_descr_t;
struct starpu_jobq_s;
struct starpu_job_s;
enum starpu_perf_archtype;

struct starpu_history_entry_t {
	//double measured;
	
	/* mean_n = 1/n sum */
	double mean;

	/* n dev_n = sum2 - 1/n (sum)^2 */
	double deviation;

	/* sum of samples */
	double sum;

	/* sum of samples^2 */
	double sum2;

//	/* sum of ln(measured) */
//	double sumlny;
//
//	/* sum of ln(size) */
//	double sumlnx;
//	double sumlnx2;
//
//	/* sum of ln(size) ln(measured) */
//	double sumlnxlny;
//
	unsigned nsample;

	uint32_t footprint;
	size_t size; /* in bytes */
};

struct starpu_history_list_t {
	struct starpu_history_list_t *next;
	struct starpu_history_entry_t *entry;
};

struct starpu_model_list_t {
	struct starpu_model_list_t *next;
	struct starpu_perfmodel_t *model;
};

//
///* File format */
//struct model_file_format {
//	unsigned ncore_entries;
//	unsigned ncuda_entries;
//	/* contains core entries, then cuda ones */
//	struct starpu_history_entry_t entries[];
//}

void _starpu_get_perf_model_dir(char *path, size_t maxlen);
void _starpu_get_perf_model_dir_codelets(char *path, size_t maxlen);
void _starpu_get_perf_model_dir_bus(char *path, size_t maxlen);
void _starpu_get_perf_model_dir_debug(char *path, size_t maxlen);

double _starpu_history_based_job_expected_length(struct starpu_perfmodel_t *model, enum starpu_perf_archtype arch, struct starpu_job_s *j);
void _starpu_register_model(struct starpu_perfmodel_t *model);
void _starpu_initialize_registered_performance_models(void);
void _starpu_deinitialize_registered_performance_models(void);

double _starpu_task_expected_length(int workerid, struct starpu_task *task, enum starpu_perf_archtype arch);
double _starpu_regression_based_job_expected_length(struct starpu_perfmodel_t *model,
					enum starpu_perf_archtype arch, struct starpu_job_s *j);
void _starpu_update_perfmodel_history(struct starpu_job_s *j, enum starpu_perf_archtype arch,
				unsigned cpuid, double measured);

double _starpu_data_expected_penalty(uint32_t memory_node, struct starpu_task *task);

void _starpu_create_sampling_directory_if_needed(void);

void _starpu_load_bus_performance_files(void);
double _starpu_predict_transfer_time(unsigned src_node, unsigned dst_node, size_t size);

void _starpu_set_calibrate_flag(unsigned val);
unsigned _starpu_get_calibrate_flag(void);

double _starpu_worker_get_relative_speedup(int workerid);

enum starpu_perf_archtype starpu_worker_get_perf_archtype(int workerid);

#if defined(STARPU_USE_CUDA)
int *_starpu_get_cuda_affinity_vector(unsigned gpuid);
#endif
#if defined(STARPU_USE_OPENCL)
int *_starpu_get_opencl_affinity_vector(unsigned gpuid);
#endif

#endif // __PERFMODEL_H__
