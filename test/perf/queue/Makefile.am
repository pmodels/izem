# -*- Mode: Makefile; -*-
#
# See COPYRIGHT in top-level directory.
#

TESTS = \
	thread_scale_mpmc_gl \
	thread_scale_mpmc_ms \
	thread_scale_mpsc_gl \
	thread_scale_mpsc_nm \
	thread_scale_mpsc_ms \
	thread_scale_spmc_gl \
	thread_scale_spmc_ms

XFAIL_TESTS =

check_PROGRAMS = $(TESTS)
noinst_PROGRAMS = $(TESTS)

include $(top_srcdir)/test/Makefile.mk

thread_scale_mpmc_gl_SOURCES = thread_scale.c
thread_scale_mpmc_ms_SOURCES = thread_scale.c
thread_scale_mpsc_gl_SOURCES = thread_scale.c
thread_scale_mpsc_nm_SOURCES = thread_scale.c
thread_scale_mpsc_ms_SOURCES = thread_scale.c
thread_scale_spmc_gl_SOURCES = thread_scale.c
thread_scale_spmc_ms_SOURCES = thread_scale.c

thread_scale_mpmc_gl_CFLAGS = -DZMTEST_USE_GLQUEUE -DZMTEST_MPMC -DZMTEST_ALLOC_QELEM -fopenmp
thread_scale_mpmc_ms_CFLAGS = -DZMTEST_USE_MSQUEUE -DZMTEST_MPMC -DZMTEST_ALLOC_QELEM -fopenmp
thread_scale_mpsc_gl_CFLAGS = -DZMTEST_USE_GLQUEUE -DZMTEST_MPSC -DZMTEST_ALLOC_QELEM -fopenmp
thread_scale_mpsc_nm_CFLAGS = -DZMTEST_USE_NMQUEUE -DZMTEST_MPSC -DZMTEST_ALLOC_QELEM -fopenmp
thread_scale_mpsc_ms_CFLAGS = -DZMTEST_USE_MSQUEUE -DZMTEST_MPSC -DZMTEST_ALLOC_QELEM -fopenmp
thread_scale_spmc_gl_CFLAGS = -DZMTEST_USE_GLQUEUE -DZMTEST_SPMC -DZMTEST_ALLOC_QELEM -fopenmp
thread_scale_spmc_ms_CFLAGS = -DZMTEST_USE_MSQUEUE -DZMTEST_SPMC -DZMTEST_ALLOC_QELEM -fopenmp

thread_scale_mpmc_gl_LDFLAGS = -fopenmp
thread_scale_mpmc_ms_LDFLAGS = -fopenmp
thread_scale_mpsc_gl_LDFLAGS = -fopenmp
thread_scale_mpsc_nm_LDFLAGS = -fopenmp
thread_scale_mpsc_ms_LDFLAGS = -fopenmp
thread_scale_spmc_gl_LDFLAGS = -fopenmp
thread_scale_spmc_ms_LDFLAGS = -fopenmp

testing:
	./thread_scale_mpmc_gl \
	./thread_scale_mpmc_ms \
	./thread_scale_mpsc_gl \
	./thread_scale_mpsc_nm \
	./thread_scale_mpsc_ms \
	./thread_scale_spmc_gl \
	./thread_scale_spmc_ms
