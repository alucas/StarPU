#
# StarPU
# Copyright (C) INRIA 2008-2009 (see AUTHORS file)
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

CC=mpicc

if USE_CUDA
# TODO define NVCCFLAGS
NVCC ?= nvcc

.cu.cubin:
	$(MKDIR_P) `dirname $@`
	$(NVCC) -cubin $< -o $@ --compiler-options -fno-strict-aliasing  $(NVCCFLAGS)

.cu.o:
	$(NVCC) $< -c -o $@ --compiler-options -fno-strict-aliasing  $(NVCCFLAGS) -I$(top_srcdir)/include/
endif


LIBS = $(top_builddir)/src/libstarpu.la @LIBS@
AM_CPPFLAGS = -I$(top_srcdir)/include/ -I$(top_srcdir)/mpi/ -I$(top_srcdir)/src/

lib_LTLIBRARIES = libstarpumpi.la

libstarpumpi_la_LIBADD = $(top_builddir)/src/libstarpu.la

noinst_HEADERS =					\
	starpu_mpi.h					\
	starpu_mpi_datatype.h

libstarpumpi_la_SOURCES =				\
	starpu_mpi.c					\
	starpu_mpi_datatype.c


TESTS = $(check_PROGRAMS)

check_PROGRAMS =

check_PROGRAMS +=					\
	tests/pingpong					\
	tests/ring					\
	tests/block_interface

tests_pingpong_LDADD =					\
	libstarpumpi.la

tests_pingpong_SOURCES =				\
	tests/pingpong.c

tests_ring_LDADD =					\
	libstarpumpi.la

tests_ring_SOURCES =					\
	tests/ring.c

tests_block_interface_LDADD =				\
	libstarpumpi.la

tests_block_interface_SOURCES =				\
	tests/block_interface.c

if USE_CUDA
tests_ring_SOURCES += tests/ring_kernel.cu
endif