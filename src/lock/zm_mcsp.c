/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdlib.h>
#include "lock/zm_mcsp.h"

int zm_mcsp_init(zm_mcsp_t *L) {
    zm_mcs_init(&L->high_p);
    L->go_straight = 0;
    L->low_p_acq = 0;
    zm_ticket_init(&L->filter);
    zm_mcs_init(&L->low_p);
    return 0;
}

int zm_mcsp_acquire(zm_mcsp_t *L, zm_mcs_qnode_t* I) {
    zm_mcs_acquire(&L->high_p, I);
    if (!L->go_straight) {
        zm_ticket_acquire(&L->filter);
        L->go_straight = 1;
    }
    return 0;
}

int zm_mcsp_acquire_low(zm_mcsp_t *L, zm_mcs_qnode_t* I) {
    zm_mcs_acquire(&L->low_p, I);
    zm_ticket_acquire(&L->filter);
    L->low_p_acq = 1;
    return 0;
}

int zm_mcsp_release(zm_mcsp_t *L, zm_mcs_qnode_t *I) {
    if (!L->low_p_acq) {
        if (zm_atomic_load(&I->next, zm_memord_acquire) == ZM_NULL) {
            L->go_straight = 0;
            zm_ticket_release(&L->filter);
        }
        zm_mcs_release(&L->high_p, I);
    } else {
        L->low_p_acq = 0;
        zm_ticket_release(&L->filter);
        zm_mcs_release(&L->low_p, I);
    }
    return 0;
}
