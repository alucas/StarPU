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

#ifndef __STARPU_UTIL_H__
#define __STARPU_UTIL_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <starpu_config.h>
#include <starpu_task.h>

#ifdef STARPU_USE_CUDA
#include <cuda.h>
#include <cuda_runtime_api.h>
#include <cublas.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define STARPU_POISON_PTR	((void *)0xdeadbeef)

#define STARPU_MIN(a,b)	((a)<(b)?(a):(b))
#define STARPU_MAX(a,b)	((a)<(b)?(b):(a))

#ifdef STARPU_NO_ASSERT
#define STARPU_ASSERT(x)	do {} while(0);
#else
#define STARPU_ASSERT(x)	assert(x)
#endif

#define STARPU_ABORT()		abort()

#define STARPU_UNLIKELY(expr)          (__builtin_expect(!!(expr),0))
#define STARPU_LIKELY(expr)            (__builtin_expect(!!(expr),1))

#if defined(__i386__) || defined(__x86_64__)
static inline unsigned starpu_cmpxchg(unsigned *ptr, unsigned old, unsigned next) {
	__asm__ __volatile__("lock cmpxchgl %2,%1": "+a" (old), "+m" (*ptr) : "q" (next) : "memory");
	return old;
}
static inline unsigned starpu_xchg(unsigned *ptr, unsigned next) {
	/* Note: xchg is always locked already */
	__asm__ __volatile__("xchgl %1,%0": "+m" (*ptr), "+q" (next) : : "memory");
	return next;
}
#define STARPU_HAVE_XCHG
#endif

#define STARPU_ATOMIC_SOMETHING(name,expr) \
static inline unsigned starpu_atomic_##name(unsigned *ptr, unsigned value) { \
	unsigned old, next; \
	while (1) { \
		old = *ptr; \
		next = expr; \
		if (starpu_cmpxchg(ptr, old, next) == old) \
			break; \
	}; \
	return expr; \
}

#ifdef STARPU_HAVE_SYNC_FETCH_AND_ADD
#define STARPU_ATOMIC_ADD(ptr, value)  (__sync_fetch_and_add ((ptr), (value)) + (value))
#elif defined(STARPU_HAVE_XCHG)
STARPU_ATOMIC_SOMETHING(add, old + value)
#define STARPU_ATOMIC_ADD(ptr, value) starpu_atomic_add(ptr, value)
#else
#error __sync_fetch_and_add is not available
#endif

#ifdef STARPU_HAVE_SYNC_FETCH_AND_OR
#define STARPU_ATOMIC_OR(ptr, value)  (__sync_fetch_and_or ((ptr), (value)))
#elif defined(STARPU_HAVE_XCHG)
STARPU_ATOMIC_SOMETHING(or, old | value)
#define STARPU_ATOMIC_OR(ptr, value) starpu_atomic_or(ptr, value)
#else
#error __sync_fetch_and_or is not available
#endif

#ifdef STARPU_HAVE_SYNC_BOOL_COMPARE_AND_SWAP
#define STARPU_BOOL_COMPARE_AND_SWAP(ptr, old, value)  (__sync_bool_compare_and_swap ((ptr), (old), (value)))
#elif defined(STARPU_HAVE_XCHG)
#define STARPU_BOOL_COMPARE_AND_SWAP(ptr, old, value) (starpu_cmpxchg((ptr), (old), (value)) == (old))
#else
#error __sync_bool_compare_and_swap is not available
#endif

#ifdef STARPU_HAVE_SYNC_LOCK_TEST_AND_SET
#define STARPU_TEST_AND_SET(ptr, value) (__sync_lock_test_and_set ((ptr), (value)))
#define STARPU_RELEASE(ptr) (__sync_lock_release ((ptr)))
#elif defined(STARPU_HAVE_XCHG)
#define STARPU_TEST_AND_SET(ptr, value) (starpu_xchg((ptr), (value)))
#define STARPU_RELEASE(ptr) (starpu_xchg((ptr), 0))
#else
#error __sync_lock_test_and_set is not available
#endif

#ifdef STARPU_HAVE_SYNC_SYNCHRONIZE
#define STARPU_SYNCHRONIZE() __sync_synchronize()
#elif defined(__i386__)
#define STARPU_SYNCHRONIZE() __asm__ __volatile__("lock; addl $0,0(%%esp)" ::: "memory")
#elif defined(__x86_64__)
#define STARPU_SYNCHRONIZE() __asm__ __volatile__("mfence" ::: "memory")
#elif defined(__ppc__) || defined(__ppc64__)
#define STARPU_SYNCHRONIZE() __asm__ __volatile__("sync" ::: "memory")
#else
#error __sync_synchronize is not available
#endif

#ifdef STARPU_USE_CUDA

#define STARPU_CUBLAS_REPORT_ERROR(status) 					\
	do {								\
		char *errormsg;						\
		switch (status) {					\
			case CUBLAS_STATUS_SUCCESS:			\
				errormsg = "success";			\
				break;					\
			case CUBLAS_STATUS_NOT_INITIALIZED:		\
				errormsg = "not initialized";		\
				break;					\
			case CUBLAS_STATUS_ALLOC_FAILED:		\
				errormsg = "alloc failed";		\
				break;					\
			case CUBLAS_STATUS_INVALID_VALUE:		\
				errormsg = "invalid value";		\
				break;					\
			case CUBLAS_STATUS_ARCH_MISMATCH:		\
				errormsg = "arch mismatch";		\
				break;					\
			case CUBLAS_STATUS_EXECUTION_FAILED:		\
				errormsg = "execution failed";		\
				break;					\
			case CUBLAS_STATUS_INTERNAL_ERROR:		\
				errormsg = "internal error";		\
				break;					\
			default:					\
				errormsg = "unknown error";		\
				break;					\
		}							\
		printf("oops  in %s ... %s \n", __func__, errormsg);	\
		assert(0);						\
	} while (0)  



#define STARPU_CUDA_REPORT_ERROR(status) 				\
	do {								\
		const char *errormsg = cudaGetErrorString(status);	\
		printf("oops  in %s ... %s \n", __func__, errormsg);	\
		assert(0);						\
	} while (0)  

#endif // STARPU_USE_CUDA

static inline int starpu_get_env_number(const char *str)
{
	char *strval;

	strval = getenv(str);
	if (strval) {
		/* the env variable was actually set */
		unsigned val;
		char *check;

		val = (int)strtol(strval, &check, 10);
		STARPU_ASSERT(strcmp(check, "\0") == 0);

		//fprintf(stderr, "ENV %s WAS %d\n", str, val);
		return val;
	}
	else {
		/* there is no such env variable */
		//fprintf("There was no %s ENV\n", str);
		return -1;
	}
}

/* Add an event in the execution trace if FxT is enabled */
void starpu_trace_user_event(unsigned code);

/* Some helper functions for application using CUBLAS kernels */
void starpu_helper_init_cublas(void);
void starpu_helper_shutdown_cublas(void);

/* Call func(arg) on every worker matching the "where" mask (eg.
 * STARPU_CUDA|STARPU_CPU to execute the function on every CPU and every CUDA
 * device). This function is synchronous, but the different workers may execute
 * the function in parallel.
 * */
void starpu_execute_on_each_worker(void (*func)(void *), void *arg, uint32_t where);

/* This creates (and submits) an empty task that unlocks a tag once all its
 * dependencies are fulfilled. */
void starpu_create_sync_task(starpu_tag_t sync_tag, unsigned ndeps, starpu_tag_t *deps,
				void (*callback)(void *), void *callback_arg);

#ifdef STARPU_USE_CUDA
cudaStream_t *starpu_get_local_cuda_stream(void);
#endif

/* If FILE is currently on a comment line, eat it.  */
void starpu_drop_comments(FILE *f);

#ifdef __cplusplus
}
#endif

#endif // __STARPU_UTIL_H__