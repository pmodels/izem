# -*- Mode: Makefile; -*-
#
# See COPYRIGHT in top-level directory.
#

TESTS = \
	cs_thruput_tkt \
	cs_thruput_mcs \
	cs_thruput_mmcs \
	cs_thruput_tlp \
	cs_thruput_hmcs

XFAIL_TESTS =

check_PROGRAMS = $(TESTS)
noinst_PROGRAMS = $(TESTS)

include $(top_srcdir)/test/Makefile.mk

cs_thruput_tkt_SOURCES = cs_thruput.c
cs_thruput_mcs_SOURCES = cs_thruput.c
cs_thruput_mmcs_SOURCES = cs_thruput.c
cs_thruput_tlp_SOURCES = cs_thruput.c
cs_thruput_hmcs_SOURCES = cs_thruput.c

cs_thruput_tkt_CFLAGS = -DZMTEST_USE_TICKET -D_GNU_SOURCE
cs_thruput_mcs_CFLAGS = -DZMTEST_USE_MCS -D_GNU_SOURCE
cs_thruput_mmcs_CFLAGS = -DZMTEST_USE_MMCS -D_GNU_SOURCE
cs_thruput_tlp_CFLAGS = -DZMTEST_USE_TLP -D_GNU_SOURCE
cs_thruput_hmcs_CFLAGS = -DZMTEST_USE_HMCS -D_GNU_SOURCE

cs_thruput_tkt_LDFLAGS = -pthread
cs_thruput_mcs_LDFLAGS = -pthread
cs_thruput_mmcs_LDFLAGS = -pthread
cs_thruput_tlp_LDFLAGS = -pthread -lstdc++
cs_thruput_hmcs_LDFLAGS = -pthread -lstdc++
