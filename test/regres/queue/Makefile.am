# -*- Mode: Makefile; -*-
#
# See COPYRIGHT in top-level directory.
#

TESTS = \
	dequeue_count_mpmc_gl \
	dequeue_count_mpmc_ms \
	dequeue_count_mpsc_gl \
	dequeue_count_mpsc_swp \
	dequeue_count_mpsc_ms \
	dequeue_count_spmc_gl \
	dequeue_count_spmc_ms \
	dequeue_count_mpsc_fa \
	deq_count_mpb \
	deq_count_mpb_bulk \
	deq_count_mpb_range

#XFAIL_TESTS = dequeue_count_mpsc_fa

check_PROGRAMS = $(TESTS)
noinst_PROGRAMS = $(TESTS)

include $(top_srcdir)/test/Makefile.mk

dequeue_count_mpmc_gl_SOURCES = dequeue_count.c
dequeue_count_mpmc_ms_SOURCES = dequeue_count.c
dequeue_count_mpsc_gl_SOURCES = dequeue_count.c
dequeue_count_mpsc_swp_SOURCES = dequeue_count.c
dequeue_count_mpsc_fa_SOURCES = dequeue_count.c
dequeue_count_mpsc_ms_SOURCES = dequeue_count.c
dequeue_count_spmc_gl_SOURCES = dequeue_count.c
dequeue_count_spmc_ms_SOURCES = dequeue_count.c
deq_count_mpb_SOURCES        = deq_count_mpb.c
deq_count_mpb_bulk_SOURCES   = deq_count_mpb.c
deq_count_mpb_range_SOURCES  = deq_count_mpb.c

dequeue_count_mpmc_gl_CFLAGS = -DZMTEST_USE_GLQUEUE -DZMTEST_MPMC -DZMTEST_ALLOC_QELEM
dequeue_count_mpmc_ms_CFLAGS = -DZMTEST_USE_MSQUEUE -DZMTEST_MPMC -DZMTEST_ALLOC_QELEM
dequeue_count_mpsc_gl_CFLAGS = -DZMTEST_USE_GLQUEUE -DZMTEST_MPSC -DZMTEST_ALLOC_QELEM
dequeue_count_mpsc_swp_CFLAGS = -DZMTEST_USE_SWPQUEUE -DZMTEST_MPSC -DZMTEST_ALLOC_QELEM
dequeue_count_mpsc_fa_CFLAGS = -DZMTEST_USE_FAQUEUE -DZMTEST_MPSC -DZMTEST_ALLOC_QELEM -g -O0
dequeue_count_mpsc_ms_CFLAGS = -DZMTEST_USE_MSQUEUE -DZMTEST_MPSC -DZMTEST_ALLOC_QELEM
dequeue_count_spmc_gl_CFLAGS = -DZMTEST_USE_GLQUEUE -DZMTEST_SPMC -DZMTEST_ALLOC_QELEM
dequeue_count_spmc_ms_CFLAGS = -DZMTEST_USE_MSQUEUE -DZMTEST_SPMC -DZMTEST_ALLOC_QELEM
deq_count_mpb_bulk_CFLAGS    = -DZMTEST_BULK
deq_count_mpb_range_CFLAGS   = -DZMTEST_RANGE

dequeue_count_mpmc_gl_LDFLAGS = -pthread
dequeue_count_mpmc_ms_LDFLAGS = -pthread
dequeue_count_mpsc_gl_LDFLAGS = -pthread
dequeue_count_mpsc_swp_LDFLAGS = -pthread
dequeue_count_mpsc_fa_LDFLAGS = -pthread
dequeue_count_mpsc_ms_LDFLAGS = -pthread
dequeue_count_spmc_gl_LDFLAGS = -pthread
dequeue_count_spmc_ms_LDFLAGS = -pthread
deq_count_mpb_LDFLAGS        = -pthread
deq_count_mpb_bulk_LDFLAGS   = -pthread
deq_count_mpb_range_LDFLAGS  = -pthread
