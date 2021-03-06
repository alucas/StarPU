#
# StarPU
# Copyright (C) Université Bordeaux 1, CNRS 2008-2009 (see AUTHORS file)
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; either version 2.1 of the License, or (at
# your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
# See the GNU Lesser General Public License in COPYING.LGPL for more details.
#

SUBDIRS =

CLEANFILES = *.gcno *.gcda *.linkinfo

EXTRA_DIST = dolib.c

ldflags =

if STARPU_HAVE_WINDOWS

LC_MESSAGES=C
export LC_MESSAGES

ldflags += -Xlinker --output-def -Xlinker .libs/libstarpu.def

if STARPU_HAVE_MS_LIB
.libs/libstarpu.lib: libstarpu.la dolib
	./dolib "$(STARPU_MS_LIB)" X86 .libs/libstarpu.def libstarpu-0 .libs/libstarpu.lib
all-local: .libs/libstarpu.lib
endif STARPU_HAVE_MS_LIB

install-exec-hook:
	$(INSTALL) .libs/libstarpu.def $(DESTDIR)$(libdir)
if STARPU_HAVE_MS_LIB
	$(INSTALL) .libs/libstarpu.lib $(DESTDIR)$(libdir)
	$(INSTALL) .libs/libstarpu.exp $(DESTDIR)$(libdir)
endif STARPU_HAVE_MS_LIB

endif STARPU_HAVE_WINDOWS

lib_LTLIBRARIES = libstarpu.la

libstarpu_la_CPPFLAGS = -I$(top_srcdir)/include/

libstarpu_la_CFLAGS = -W -Wall -Wextra $(HWLOC_CFLAGS)
libstarpu_la_LIBADD = -lm $(HWLOC_LIBS)
libstarpu_la_LDFLAGS = $(ldflags) -version-number 0:0:0 -no-undefined

noinst_HEADERS = 						\
	core/dependencies/data_concurrency.h			\
	core/dependencies/implicit_data_deps.h			\
	core/sched_policy.h					\
	core/perfmodel/perfmodel.h				\
	core/perfmodel/regression.h				\
	core/jobs.h						\
	core/trigger.h						\
	core/task.h						\
	core/workers.h						\
	core/topology.h						\
	core/debug.h						\
	core/event.h						\
	core/errorcheck.h					\
	sched_policies/fifo_queues.h				\
	sched_policies/deque_queues.h				\
	sched_policies/stack_queues.h				\
	datawizard/footprint.h					\
	datawizard/datawizard.h					\
	datawizard/data_request.h				\
	datawizard/filters.h					\
	datawizard/write_back.h					\
	datawizard/datastats.h					\
	datawizard/memalloc.h					\
	datawizard/copy_driver.h				\
	datawizard/coherency.h					\
	datawizard/sort_data_handles.h				\
	datawizard/memory_nodes.h				\
	datawizard/interfaces/data_interface.h			\
	common/common.h						\
	common/hash.h						\
	common/timing.h						\
	common/htable32.h					\
	common/list.h						\
	common/rwlock.h						\
	common/starpu_spinlock.h				\
	common/fxt.h						\
	common/utils.h						\
	drivers/common/common.h			\
	drivers/cpu/cpu.h				\
	drivers/gordon/gordon.h				\
	drivers/gordon/gordon_interface.h			\
	profiling/bound.h					\
	drivers/cuda/cuda.h				\
	drivers/cuda/event.h				\
	drivers/opencl/opencl.h				\
	drivers/opencl/event.h				\
	drivers/opencl/utils.h			\
	profiling/profiling.h

libstarpu_la_SOURCES = 						\
	common/hash.c 						\
	common/htable32.c					\
	common/rwlock.c						\
	common/starpu_spinlock.c				\
	common/timing.c						\
	common/fxt.c						\
	common/utils.c						\
	core/jobs.c						\
	core/trigger.c						\
	core/task.c						\
	core/workers.c						\
	core/topology.c						\
	core/debug.c						\
	core/event.c						\
	core/event_group.c						\
	core/event_callback.c						\
	core/errorcheck.c					\
	core/progress_hook.c					\
	core/dependencies/implicit_data_deps.c			\
	core/dependencies/task_deps.c				\
	core/dependencies/data_concurrency.c			\
	core/perfmodel/perfmodel_history.c			\
	core/perfmodel/perfmodel_bus.c				\
	core/perfmodel/perfmodel.c				\
	core/perfmodel/regression.c				\
	core/sched_policy.c					\
	core/priorities.c					\
	sched_policies/eager_central_policy.c			\
	sched_policies/eager_central_priority_policy.c		\
	sched_policies/work_stealing_policy.c			\
	sched_policies/deque_modeling_policy_data_aware.c	\
	sched_policies/random_policy.c				\
	sched_policies/stack_queues.c				\
	sched_policies/deque_queues.c				\
	sched_policies/fifo_queues.c				\
	drivers/common/common.c			\
	datawizard/memory_nodes.c				\
	datawizard/write_back.c					\
	datawizard/coherency.c					\
	datawizard/data_request.c				\
	datawizard/progress.c					\
	datawizard/copy_driver.c				\
	datawizard/filters.c					\
	datawizard/sort_data_handles.c				\
	datawizard/memalloc.c					\
	datawizard/footprint.c					\
	datawizard/datastats.c					\
	datawizard/user_interactions.c				\
	datawizard/interfaces/data_interface.c			\
	datawizard/interfaces/bcsr_interface.c			\
	datawizard/interfaces/csr_interface.c			\
	datawizard/interfaces/matrix_filters.c			\
	datawizard/interfaces/matrix_interface.c		\
	datawizard/interfaces/block_filters.c			\
	datawizard/interfaces/block_interface.c			\
	datawizard/interfaces/vector_interface.c		\
	datawizard/interfaces/bcsr_filters.c			\
	datawizard/interfaces/csr_filters.c			\
	datawizard/interfaces/vector_filters.c			\
	datawizard/interfaces/variable_interface.c		\
	util/malloc.c						\
	util/execute_on_all.c					\
	util/starpu_cublas.c					\
	util/file.c						\
	util/starpu_insert_task.c				\
	util/starpu_task_list.c					\
	debug/latency.c						\
	profiling/profiling.c					\
	profiling/bound.c					\
	profiling/bus_profiling_helpers.c

if STARPU_USE_CPU
libstarpu_la_SOURCES += drivers/cpu/cpu.c
endif

if STARPU_USE_CUDA
libstarpu_la_SOURCES += drivers/cuda/cuda.c \
                        drivers/cuda/event.c
endif

if STARPU_USE_GORDON
libstarpu_la_SOURCES += drivers/gordon/gordon.c
endif

if STARPU_USE_OPENCL
libstarpu_la_SOURCES += drivers/opencl/opencl.c       \
                        drivers/opencl/event.c \
                        drivers/opencl/utils.c
endif
