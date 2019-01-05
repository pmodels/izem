# -*- Mode: Makefile; -*-
#
# See COPYRIGHT in top-level directory.
#

TESTS = \
	enq_deq_pairs_gl \
	enq_deq_pairs_ms \
	enq_deq_pairs_swp \
	thread_scale_mpmc_gl \
	thread_scale_mpmc_ms \
	thread_scale_mpmc_dsm \
	thread_scale_mpmc_idsm \
	thread_scale_mpmc_wf \
	thread_scale_mpsc_gl \
	thread_scale_mpsc_swp \
	thread_scale_mpsc_iswp \
	thread_scale_mpsc_ms \
	thread_scale_mpsc_dsm \
	thread_scale_mpsc_wf \
	thread_scale_spmc_gl \
	thread_scale_spmc_ms \
	enq_deq_pairs_fa \
	thread_scale_mpsc_fa \
	thscale_mpb

#XFAIL_TESTS = 	enq_deq_pairs_fa \
#		thread_scale_mpsc_fa

check_PROGRAMS = $(TESTS)
noinst_PROGRAMS = $(TESTS)

include $(top_srcdir)/test/Makefile.mk

enq_deq_pairs_gl_SOURCES = enq_deq_pairs.c
enq_deq_pairs_ms_SOURCES = enq_deq_pairs.c
enq_deq_pairs_swp_SOURCES = enq_deq_pairs.c
enq_deq_pairs_fa_SOURCES = enq_deq_pairs.c
thread_scale_mpmc_gl_SOURCES = thread_scale.c
thread_scale_mpmc_ms_SOURCES = thread_scale.c
thread_scale_mpmc_dsm_SOURCES = thread_scale.c
thread_scale_mpmc_idsm_SOURCES = thread_scale.c
thread_scale_mpmc_wf_SOURCES = thread_scale.c
thread_scale_mpsc_gl_SOURCES = thread_scale.c
thread_scale_mpsc_swp_SOURCES = thread_scale.c
thread_scale_mpsc_iswp_SOURCES = thread_scale.c
thread_scale_mpsc_fa_SOURCES = thread_scale.c
thread_scale_mpsc_ms_SOURCES = thread_scale.c
thread_scale_mpsc_dsm_SOURCES = thread_scale.c
thread_scale_mpsc_wf_SOURCES = thread_scale.c
thread_scale_spmc_gl_SOURCES = thread_scale.c
thread_scale_spmc_ms_SOURCES = thread_scale.c
thscale_mpb_SOURCES 		 = thscale_mpb.c

enq_deq_pairs_gl_CFLAGS = -DZM_QUEUE_CONF=ZM_GLQUEUE_IF -DZMTEST_ALLOC_QELEM -fopenmp
enq_deq_pairs_ms_CFLAGS = -DZM_QUEUE_CONF=ZM_MSQUEUE_IF -DZMTEST_ALLOC_QELEM -fopenmp
enq_deq_pairs_swp_CFLAGS = -DZM_QUEUE_CONF=ZM_SWPQUEUE_IF -DZMTEST_ALLOC_QELEM -fopenmp
enq_deq_pairs_fa_CFLAGS = -DZM_QUEUE_CONF=ZM_FAQUEUE_IF -DZMTEST_ALLOC_QELEM -fopenmp
thread_scale_mpmc_gl_CFLAGS = -DZM_QUEUE_CONF=ZM_GLQUEUE_IF -DZMTEST_MPMC -DZMTEST_ALLOC_QELEM -fopenmp
thread_scale_mpmc_ms_CFLAGS = -DZM_QUEUE_CONF=ZM_MSQUEUE_IF -DZMTEST_MPMC -DZMTEST_ALLOC_QELEM -fopenmp
thread_scale_mpmc_dsm_CFLAGS = -DZM_QUEUE_CONF=ZM_DSMQUEUE_IF -DZMTEST_MPMC -DZMTEST_ALLOC_QELEM -fopenmp
thread_scale_mpmc_idsm_CFLAGS = -DZM_QUEUE_CONF=ZM_IDSMQUEUE_IF -DZMTEST_MPMC -DZMTEST_ALLOC_QELEM -fopenmp
thread_scale_mpmc_wf_CFLAGS = -DZM_QUEUE_CONF=ZM_WFQUEUE_IF -DZMTEST_MPMC -DZMTEST_ALLOC_QELEM -fopenmp
thread_scale_mpsc_gl_CFLAGS = -DZM_QUEUE_CONF=ZM_GLQUEUE_IF -DZMTEST_MPSC -DZMTEST_ALLOC_QELEM -fopenmp
thread_scale_mpsc_swp_CFLAGS = -DZM_QUEUE_CONF=ZM_SWPQUEUE_IF -DZMTEST_MPSC -fopenmp
thread_scale_mpsc_iswp_CFLAGS = -DZM_QUEUE_CONF=ZM_ISWPQUEUE_IF -DZMTEST_MPSC -fopenmp
thread_scale_mpsc_fa_CFLAGS = -DZM_QUEUE_CONF=ZM_FAQUEUE_IF -DZMTEST_MPSC -DZMTEST_ALLOC_QELEM -fopenmp
thread_scale_mpsc_ms_CFLAGS = -DZM_QUEUE_CONF=ZM_MSQUEUE_IF -DZMTEST_MPSC -DZMTEST_ALLOC_QELEM -fopenmp
thread_scale_mpsc_dsm_CFLAGS = -DZM_QUEUE_CONF=ZM_DSMQUEUE_IF -DZMTEST_MPSC -DZMTEST_ALLOC_QELEM -fopenmp
thread_scale_mpsc_wf_CFLAGS = -DZM_QUEUE_CONF=ZM_WFQUEUE_IF -DZMTEST_MPSC -DZMTEST_ALLOC_QELEM -fopenmp
thread_scale_spmc_gl_CFLAGS = -DZM_QUEUE_CONF=ZM_GLQUEUE_IF -DZMTEST_SPMC -DZMTEST_ALLOC_QELEM -fopenmp
thread_scale_spmc_ms_CFLAGS = -DZM_QUEUE_CONF=ZM_MSQUEUE_IF -DZMTEST_SPMC -DZMTEST_ALLOC_QELEM -fopenmp
thscale_mpb_CFLAGS 		= -fopenmp

enq_deq_pairs_gl_LDFLAGS = -fopenmp
enq_deq_pairs_ms_LDFLAGS = -fopenmp
enq_deq_pairs_swp_LDFLAGS = -fopenmp
enq_deq_pairs_fa_LDFLAGS = -fopenmp
thread_scale_mpmc_gl_LDFLAGS = -fopenmp
thread_scale_mpmc_ms_LDFLAGS = -fopenmp
thread_scale_mpmc_dsm_LDFLAGS = -fopenmp
thread_scale_mpmc_idsm_LDFLAGS = -fopenmp
thread_scale_mpmc_wf_LDFLAGS = -fopenmp
thread_scale_mpsc_gl_LDFLAGS = -fopenmp
thread_scale_mpsc_swp_LDFLAGS = -fopenmp
thread_scale_mpsc_iswp_LDFLAGS = -fopenmp
thread_scale_mpsc_fa_LDFLAGS = -fopenmp
thread_scale_mpsc_ms_LDFLAGS = -fopenmp
thread_scale_mpsc_dsm_LDFLAGS = -fopenmp
thread_scale_mpsc_wf_LDFLAGS = -fopenmp
thread_scale_spmc_gl_LDFLAGS = -fopenmp
thread_scale_spmc_ms_LDFLAGS = -fopenmp
thscale_mpb_LDFLAGS = -fopenmp
