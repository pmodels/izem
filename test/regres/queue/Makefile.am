# -*- Mode: Makefile; -*-
#
# See COPYRIGHT in top-level directory.
#

TESTS = \
	two_sided_funnel_gl \
	two_sided_funnel_ms

XFAIL_TESTS =

check_PROGRAMS = $(TESTS)
noinst_PROGRAMS = $(TESTS)

include $(top_srcdir)/test/Makefile.mk

two_sided_funnel_gl_SOURCES = two_sided_funnel.c
two_sided_funnel_ms_SOURCES = two_sided_funnel.c

two_sided_funnel_gl_CFLAGS = -DZMTEST_USE_GLQUEUE
two_sided_funnel_ms_CFLAGS = -DZMTEST_USE_MSQUEUE

two_sided_funnel_gl_LDFLAGS = -pthread
two_sided_funnel_ms_LDFLAGS = -pthread

testing:
	./two_sided_funnel
