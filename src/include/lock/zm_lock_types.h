#ifndef _ZM_LOCK_TYPES_H
#define _ZM_LOCK_TYPES_H

#if !defined(__STDC_NO_ATOMICS__)
#include <stdatomic.h>
#else
#include <opa_primitives.h>
#endif

typedef struct zm_ticket zm_ticket_t;

struct zm_ticket {
    atomic_uint next_ticket;
    atomic_uint now_serving;
};

#endif /* _IZEM_LOCK_TYPES_H */
