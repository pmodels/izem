# -*- Mode: Makefile; -*-
#
# See COPYRIGHT in top-level directory.
#

TESTS = \
	cs_thruput_tkt \
	cs_thruput_mcs \
	cs_thruput_tlp \
	cs_thruput_hmcs\
	cs_thruput_dsm\
	tryacq_tkt \
	tryacq_mcs \
	tryacq_tlp \
	tryacq_hmcs \
	hmpr_thruput

XFAIL_TESTS =

check_PROGRAMS = $(TESTS)
noinst_PROGRAMS = $(TESTS)

include $(top_srcdir)/test/Makefile.mk

cs_thruput_tkt_SOURCES = cs_thruput.c
cs_thruput_mcs_SOURCES = cs_thruput.c
cs_thruput_tlp_SOURCES = cs_thruput.c
cs_thruput_hmcs_SOURCES = cs_thruput.c
cs_thruput_dsm_SOURCES = cs_thruput.c
tryacq_tkt_SOURCES = cs_thruput.c
tryacq_mcs_SOURCES = cs_thruput.c
tryacq_tlp_SOURCES = cs_thruput.c
tryacq_hmcs_SOURCES = cs_thruput.c
hmpr_thruput_SOURCES = hmpr_thruput.c

cs_thruput_tkt_CFLAGS = -DZM_LOCK_IF=ZM_TICKET_IF -D_GNU_SOURCE
cs_thruput_mcs_CFLAGS = -DZM_LOCK_IF=ZM_MCS_IF -D_GNU_SOURCE
cs_thruput_tlp_CFLAGS = -DZM_LOCK_IF=ZM_TLP_IF  -D_GNU_SOURCE
cs_thruput_hmcs_CFLAGS = -DZM_LOCK_IF=ZM_HMCS_IF -D_GNU_SOURCE
cs_thruput_dsm_CFLAGS = -DZM_LOCK_IF=ZM_DSM_IF -D_GNU_SOURCE
tryacq_tkt_CFLAGS = -DZM_LOCK_IF=ZM_TICKET_IF -D_GNU_SOURCE
tryacq_mcs_CFLAGS = -DZM_LOCK_IF=ZM_MCS_IF -D_GNU_SOURCE
tryacq_tlp_CFLAGS = -DZM_LOCK_IF=ZM_TLP_IF -D_GNU_SOURCE
tryacq_hmcs_CFLAGS = -DZM_LOCK_IF=ZM_HMCS_IF -D_GNU_SOURCE
hmpr_thruput_CFLAGS = -D_GNU_SOURCE

cs_thruput_tkt_LDFLAGS = -pthread
cs_thruput_mcs_LDFLAGS = -pthread
cs_thruput_tlp_LDFLAGS = -pthread -lstdc++
cs_thruput_hmcs_LDFLAGS = -pthread -lstdc++
cs_thruput_dsm_LDFLAGS = -pthread
tryacq_tkt_LDFLAGS = -pthread
tryacq_mcs_LDFLAGS = -pthread
tryacq_tlp_LDFLAGS = -pthread
tryacq_hmcs_LDFLAGS = -pthread
hmpr_thruput_LDFLAGS = -pthread
