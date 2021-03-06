#
# StarPU
# Copyright (C) Université Bordeaux 1, CNRS 2009 (see AUTHORS file)
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
#
AM_CPPFLAGS = -I$(top_srcdir)/include/ -I$(top_builddir)/include

lib_LTLIBRARIES = libstarpufft.la

EXTRA_DIST =			\
	starpufft.h		\
	float.h			\
	double.h		\
	cudax_kernels.h		\
	starpufftx.c		\
	starpufftx1d.c		\
	starpufftx2d.c		\
	cuda_kernels.cu		\
	cudaf_kernels.cu	\
	cudax_kernels.cu	\
	testx.c			\
	testx_threads.c		\
	testf_threads.c		\
	test_threads.c

libstarpufft_la_SOURCES = starpufft.c starpufftf.c starpufft_common.c
libstarpufft_la_LIBADD = $(top_builddir)/src/libstarpu.la $(FFTW_LIBS) $(FFTWF_LIBS)
libstarpufft_la_CFLAGS = $(FFTWF_CFLAGS)

if STARPU_USE_CUDA
NVCCFLAGS += -Xcompiler -fPIC -Xlinker -fPIC

cudaf_kernels.o: cudaf_kernels.cu
	@echo "  NVCC  " $@
	@$(NVCC) $(AM_CPPFLAGS) $< -c -o $@ --compiler-options -fno-strict-aliasing  $(NVCCFLAGS) -I${includedir}

libstarpufft_la_SOURCES += cudaf_kernels.cu
am_libstarpufft_la_OBJECTS = cudaf_kernels.o starpufftf.lo starpufft_common.lo

if STARPU_HAVE_CUFFTDOUBLECOMPLEX
cuda_kernels.o: cuda_kernels.cu
	@echo "  NVCC  " $@
	@$(NVCC) $(AM_CPPFLAGS) $< -c -o $@ --compiler-options -fno-strict-aliasing  $(NVCCFLAGS) -I${includedir} -arch sm_13 

libstarpufft_la_SOURCES += cuda_kernels.cu
am_libstarpufft_la_OBJECTS += cuda_kernels.o starpufft.lo 
endif

libstarpufft_la_LIBADD += -lcufft
endif

check_PROGRAMS = testf
testf_LDADD = libstarpufft.la $(top_builddir)/src/libstarpu.la $(FFTWF_LIBS)

# If we don't have CUDA, we assume that we have fftw available in double
# precision anyway, we just want to make sure that if CUFFT is used, it also
# supports double precision. 
if !STARPU_USE_CUDA
check_PROGRAMS += test
test_LDADD = libstarpufft.la $(top_builddir)/src/libstarpu.la $(FFTW_LIBS)
else
if STARPU_HAVE_CUFFTDOUBLECOMPLEX
check_PROGRAMS += test
test_LDADD = libstarpufft.la $(top_builddir)/src/libstarpu.la $(FFTW_LIBS)
endif
endif



#check_PROGRAMS += test_threads testf_threads
#test_threads_LDADD = libstarpufft.la $(top_builddir)/src/libstarpu.la -lfftw3_threads
#testf_threads_LDADD = libstarpufft.la $(top_builddir)/src/libstarpu.la -lfftw3f_threads
