#ifndef _ZM_LOCK_TYPES_H
#define _ZM_LOCK_TYPES_H

#include "common/zm_common.h"

#define ZM_LOCKED 1
#define ZM_UNLOCKED 0

typedef struct zm_ticket zm_ticket_t;

struct zm_ticket {
    atomic_uint next_ticket;
    atomic_uint now_serving;
};

/* MCS */
typedef zm_atomic_ptr_t zm_mcs_t;
typedef struct zm_mcs_qnode zm_mcs_qnode_t;

struct zm_mcs_qnode {
    atomic_uint status;
    zm_atomic_ptr_t next;
};

/* Context Saving MCS */
typedef struct zm_csvmcs zm_csvmcs_t;

struct zm_csvmcs {
    zm_atomic_ptr_t lock;
    zm_mcs_qnode_t* cur_ctx __attribute__((aligned(64)));
};

#endif /* _IZEM_LOCK_TYPES_H */
