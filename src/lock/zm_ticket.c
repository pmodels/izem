/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "lock/zm_ticket.h"

int zm_ticket_init(zm_ticket_t *lock)
{
    atomic_store_explicit(&lock->next_ticket, 0, memory_order_release);
    atomic_store_explicit(&lock->now_serving, 0, memory_order_release);
    return 0;
}

/* Atomically increment the nex_ticket counter and get my ticket.
   Then spin on now_serving until it equals my ticket. */
int zm_ticket_acquire(zm_ticket_t* lock) {
    unsigned my_ticket = atomic_fetch_add_explicit(&lock->next_ticket, 1, memory_order_acq_rel);
    while(atomic_load_explicit(&lock->now_serving, memory_order_acquire) != my_ticket)
            ; /* SPIN */
    return 0;
}

/* Release the lock */
int zm_ticket_release(zm_ticket_t* lock) {
    atomic_fetch_add_explicit(&lock->now_serving, 1, memory_order_release);
    return 0;
}

