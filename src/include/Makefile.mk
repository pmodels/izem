# -*- Mode: Makefile; -*-
#
# See COPYRIGHT in top-level directory.
#

prefixdir = ${prefix}

nobase_prefix_HEADERS = \
			include/common/zm_common.h \
			include/mem/zm_hzdptr.h \
			include/lock/zm_lock_types.h \
			include/lock/zm_ticket.h \
			include/lock/zm_mcs.h \
			include/lock/zm_csvmcs.h \
			include/list/zm_sdlist.h \
			include/queue/zm_queue_types.h \
			include/queue/zm_glqueue.h \
			include/queue/zm_msqueue.h

noinst_HEADERS = \
		include/zm_config.h
