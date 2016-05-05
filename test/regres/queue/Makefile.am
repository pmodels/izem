# -*- Mode: Makefile; -*-
#
# See COPYRIGHT in top-level directory.
#

TESTS = \
	two_sided_funnel

XFAIL_TESTS =

check_PROGRAMS = $(TESTS)
noinst_PROGRAMS = $(TESTS)

include $(top_srcdir)/test/Makefile.mk

two_sided_funnel_SOURCES = two_sided_funnel.c

two_sided_funnel_CFLAGS = -g -O0

two_sided_funnel_LDFLAGS = -pthread

testing:
	./two_sided_funnel
