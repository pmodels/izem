# -*- Mode: Makefile; -*-
#
# See COPYRIGHT in top-level directory.
#

TESTS = \
	rr_sched \
	prio_chain

XFAIL_TESTS =

check_PROGRAMS = $(TESTS)
noinst_PROGRAMS = $(TESTS)

include $(top_srcdir)/test/Makefile.mk

rr_sched_SOURCES       = rr_sched.c
prio_chain_SOURCES     = prio_chain.c

rr_sched_CFLAGS        =

rr_sched_LDFLAGS       = -pthread
prio_chain_LDFLAGS     = -pthread
