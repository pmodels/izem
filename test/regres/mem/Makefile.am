# -*- Mode: Makefile; -*-
#
# See COPYRIGHT in top-level directory.
#

TESTS = \
	mem_pool

XFAIL_TESTS =

check_PROGRAMS = $(TESTS)
noinst_PROGRAMS = $(TESTS)

include $(top_srcdir)/test/Makefile.mk

mem_pool_SOURCES = mem_pool.c

mem_pool_CFLAGS = -D_GNU_SOURCE

mem_pool_LDFLAGS = -pthread