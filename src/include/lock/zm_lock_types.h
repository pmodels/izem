#ifndef _ZM_LOCK_TYPES_H
#define _ZM_LOCK_TYPES_H

#include "common/zm_common.h"

typedef struct zm_ticket zm_ticket_t;

struct zm_ticket {
    atomic_uint next_ticket;
    atomic_uint now_serving;
};

#endif /* _IZEM_LOCK_TYPES_H */
