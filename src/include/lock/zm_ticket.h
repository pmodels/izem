/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef _ZM_TICKET_H
#define _ZM_TICKET_H
#include "lock/zm_lock_types.h"

int zm_ticket_init(zm_ticket_t *);

/* Atomically increment the nex_ticket counter and get my ticket.
   Then spin on now_serving until it equals my ticket. */
static inline int zm_ticket_acquire(zm_ticket_t* lock) {
    unsigned my_ticket = atomic_fetch_add_explicit(&lock->next_ticket, 1, memory_order_acq_rel);
    while(atomic_load_explicit(&lock->now_serving, memory_order_acquire) != my_ticket)
            ; /* SPIN */
    return 0;
}

/* Release the lock */
static inline int zm_ticket_release(zm_ticket_t* lock) {
    atomic_fetch_add_explicit(&lock->now_serving, 1, memory_order_release);
    return 0;
}
#endif /* _ZM_TICKET_H */
