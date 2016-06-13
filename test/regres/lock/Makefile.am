# -*- Mode: Makefile; -*-
#
# See COPYRIGHT in top-level directory.
#

TESTS = \
	cs_thruput_tkt \
	cs_thruput_mcs \
	cs_thruput_cvsmcs

XFAIL_TESTS =

check_PROGRAMS = $(TESTS)
noinst_PROGRAMS = $(TESTS)

include $(top_srcdir)/test/Makefile.mk

cs_thruput_tkt_SOURCES = cs_thruput.c
cs_thruput_mcs_SOURCES = cs_thruput.c
cs_thruput_cvsmcs_SOURCES = cs_thruput.c

cs_thruput_tkt_CFLAGS = -DZMTEST_USE_TICKET
cs_thruput_mcs_CFLAGS = -DZMTEST_USE_MCS
cs_thruput_cvsmcs_CFLAGS = -DZMTEST_USE_CVSMCS

cs_thruput_tkt_LDFLAGS = -pthread
cs_thruput_mcs_LDFLAGS = -pthread
cs_thruput_cvsmcs_LDFLAGS = -pthread

testing:
	./cs_thruput_tkt \
	./cs_thruput_mcs \
	./cs_thruput_cvsmcs
