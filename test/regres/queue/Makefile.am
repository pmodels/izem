# -*- Mode: Makefile; -*-
#
# See COPYRIGHT in top-level directory.
#

TESTS = \
	dequeue_count_mpmc_gl \
	dequeue_count_mpmc_ms

XFAIL_TESTS =

check_PROGRAMS = $(TESTS)
noinst_PROGRAMS = $(TESTS)

include $(top_srcdir)/test/Makefile.mk

dequeue_count_mpmc_gl_SOURCES = dequeue_count_mpmc.c
dequeue_count_mpmc_ms_SOURCES = dequeue_count_mpmc.c

dequeue_count_mpmc_gl_CFLAGS = -DZMTEST_USE_GLQUEUE
dequeue_count_mpmc_ms_CFLAGS = -DZMTEST_USE_MSQUEUE

dequeue_count_mpmc_gl_LDFLAGS = -pthread
dequeue_count_mpmc_ms_LDFLAGS = -pthread

testing:
	./dequeue_count_mpmc
