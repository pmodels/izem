#ifndef _ZM_CSVMCS_H
#define _ZM_CSVMCS_H
#include "lock/zm_lock_types.h"

int zm_csvmcs_init(zm_csvmcs_t *);

static inline int zm_csvmcs_acquire(zm_csvmcs_t *L, zm_mcs_qnode_t* I) {
    if(I == NULL)
        I = L->cur_ctx;
    atomic_store_explicit(&I->next, NULL, memory_order_release);
    zm_mcs_qnode_t* pred = (zm_mcs_qnode_t*)atomic_exchange_explicit(&L->lock, (zm_ptr_t)I, memory_order_acq_rel);
    if(pred != NULL) {
        atomic_store_explicit(&I->status, ZM_LOCKED, memory_order_release);
        atomic_store_explicit(&pred->next, (zm_ptr_t)I, memory_order_release);
        while(atomic_load_explicit(&I->status, memory_order_acquire) != ZM_UNLOCKED)
            ; /* SPIN */
    }
    L->cur_ctx = I; /* save current local context*/
    return 0;
}

/* Release the lock */
static inline int zm_csvmcs_release(zm_csvmcs_t *L) {
    zm_mcs_qnode_t* I = L->cur_ctx; /* get current local context */
    L->cur_ctx = NULL;
    if (atomic_load_explicit(&I->next, memory_order_acquire) == NULL) {
        zm_mcs_qnode_t *tmp = I;
        if(atomic_compare_exchange_weak_explicit(&L->lock,
                                                 (zm_ptr_t*)&tmp,
                                                 NULL,
                                                 memory_order_release,
                                                 memory_order_acquire))
            return 0;
        while(atomic_load_explicit(&I->next, memory_order_acquire) == NULL)
            ; /* SPIN */
    }
    atomic_store_explicit(&((zm_mcs_qnode_t*)atomic_load_explicit(&I->next, memory_order_acquire))->status, ZM_UNLOCKED, memory_order_release);
    return 0;
}
#endif /* _ZM_CSVMCS_H */
