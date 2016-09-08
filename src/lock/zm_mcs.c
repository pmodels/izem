/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdlib.h>
#include "lock/zm_mcs.h"

int zm_mcs_init(zm_mcs_t *L)
{
    atomic_store(L, (zm_ptr_t)ZM_NULL);
    return 0;
}

int zm_mcs_acquire(zm_mcs_t *L, zm_mcs_qnode_t* I) {
    atomic_store_explicit(&I->next, ZM_NULL, memory_order_release);
    zm_mcs_qnode_t* pred = (zm_mcs_qnode_t*)atomic_exchange_explicit(L, (zm_ptr_t)I, memory_order_acq_rel);
    if((zm_ptr_t)pred != ZM_NULL) {
        atomic_store_explicit(&I->status, ZM_LOCKED, memory_order_release);
        atomic_store_explicit(&pred->next, (zm_ptr_t)I, memory_order_release);
        while(atomic_load_explicit(&I->status, memory_order_acquire) != ZM_UNLOCKED)
            ; /* SPIN */
    }
    return 0;
}

/* Release the lock */
int zm_mcs_release(zm_mcs_t *L, zm_mcs_qnode_t *I) {
    if (atomic_load_explicit(&I->next, memory_order_acquire) == ZM_NULL) {
        zm_mcs_qnode_t *tmp = I;
        if(atomic_compare_exchange_weak_explicit(L,
                                                 (zm_ptr_t*)&tmp,
                                                 ZM_NULL,
                                                 memory_order_release,
                                                 memory_order_acquire))
            return 0;
        while(atomic_load_explicit(&I->next, memory_order_acquire) == ZM_NULL)
            ; /* SPIN */
    }
    atomic_store_explicit(&((zm_mcs_qnode_t*)atomic_load_explicit(&I->next, memory_order_acquire))->status, ZM_UNLOCKED, memory_order_release);
    return 0;
}
