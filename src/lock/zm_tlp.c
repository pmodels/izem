/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdlib.h>
#include "lock/zm_tlp.h"

/* The algorithm follows this logic

acquire()
lock(high_p)
do {
    if(CAS(common, FREE, GRANTED);
        break;
    if (CAS(common, GRANTED, REQUEST {
        while(common != GRANTED) ;
        break;     }
} while (1)
unlock(high_p)

CS

release()
if(!CAS(common, REQUEST, GRANTED); //optimistic
    common = FREE

acquire_low()
lock(low_p)
do {
    while(common != FREE) ;
    if(CAS(common, FREE, GRANTED);
        break;
} whil (1)


CS

release_low()
if(!CAS(common, REQUEST, GRANTED); //optimistic
    common = FREE;
unlock(low_p)
*/

/* Helper functions */

#if (ZM_TLP_HIGH_P == ZM_TICKET)
#define zm_tlp_init_high_p(L) zm_ticket_init(&L->high_p)
#elif (ZM_TLP_HIGH_P == ZM_MCS)
#define zm_tlp_init_high_p(L) zm_mcs_init(&L->high_p)
#elif (ZM_TLP_HIGH_P == ZM_HMCS)
#define zm_tlp_init_high_p(L) zm_hmcs_init(&L->high_p)
#endif

#if (ZM_TLP_HIGH_P == ZM_TICKET)
#define zm_tlp_acquire_high_p(L, local_ctxt) zm_ticket_acquire(&L->high_p)
#elif (ZM_TLP_HIGH_P == ZM_MCS)
#define zm_tlp_acquire_high_p(L, local_ctxt) zm_mcs_acquire(&L->high_p, local_ctxt)
#elif (ZM_TLP_HIGH_P == ZM_HMCS)
#define zm_tlp_acquire_high_p(L, local_ctxt) zm_hmcs_acquire(L->high_p)
#endif

#if (ZM_TLP_HIGH_P == ZM_TICKET)
#define zm_tlp_release_high_p(L, ctxt)  zm_ticket_release(&L->high_p)
#elif (ZM_TLP_HIGH_P == ZM_MCS)
#define zm_tlp_release_high_p(L, ctxt)  zm_mcs_release(&L->high_p, ctxt);
#elif (ZM_TLP_HIGH_P == ZM_HMCS)
#define zm_tlp_release_high_p(L, ctxt)  zm_hmcs_release(L->high_p);
#endif

#if (ZM_TLP_LOW_P == ZM_TICKET)
#define zm_tlp_init_low_p(L) zm_ticket_init(&L->low_p)
#elif (ZM_TLP_LOW_P == ZM_MCS)
#define zm_tlp_init_low_p(L) zm_mcs_init(&L->low_p)
#elif (ZM_TLP_LOW_P == ZM_HMCS)
#define zm_tlp_init_low_p(L) zm_hmcs_init(&L->low_p)
#endif

#if (ZM_TLP_LOW_P == ZM_TICKET)
#define zm_tlp_acquire_low_p(L, local_ctxt) zm_ticket_acquire(&L->low_p)
#elif (ZM_TLP_LOW_P == ZM_MCS)
#define zm_tlp_acquire_low_p(L, local_ctxt) zm_mcs_acquire(&L->low_p, local_ctxt)
#elif (ZM_TLP_LOW_P == ZM_HMCS)
#define zm_tlp_acquire_low_p(L, local_ctxt) zm_hmcs_acquire(L->low_p)
#endif

#if (ZM_TLP_LOW_P == ZM_TICKET)
#define zm_tlp_release_low_p(L, ctxt) zm_ticket_release(&L->low_p)
#elif (ZM_TLP_LOW_P == ZM_MCS)
#define zm_tlp_release_low_p(L, ctxt) zm_mcs_release(&L->low_p, ctxt);
#elif (ZM_TLP_LOW_P == ZM_HMCS)
#define zm_tlp_release_low_p(L, ctxt) zm_hmcs_release(L->low_p)
#endif

#if (ZM_TLP_HIGH_P == ZM_TICKET)
#define zm_tlp_nowaiters_high_p(L, ctxt) zm_ticket_nowaiters(&L->high_p)
#elif (ZM_TLP_HIGH_P == ZM_MCS)
#define zm_tlp_nowaiters_high_p(L, ctxt) zm_mcs_nowaiters(&L->high_p, ctxt)
#elif (ZM_TLP_HIGH_P == ZM_HMCS)
#define zm_tlp_nowaiters_high_p(L, ctxt) zm_hmcs_nowaiters(L->high_p)
#endif

int zm_tlp_init(zm_tlp_t *L)
{
   zm_tlp_init_high_p(L);
   L->go_straight = 0;
   L->low_p_acq = 0;
   zm_ticket_init(&L->filter);
   zm_tlp_init_low_p(L);
   return 0;
}

int zm_tlp_acquire(zm_tlp_t *L, zm_mcs_qnode_t *local_ctxt) {
    /* Acquire the high priority lock */
   zm_tlp_acquire_high_p(L, local_ctxt);
   if (!L->go_straight) {
       zm_ticket_acquire(&L->filter);
       L->go_straight = 1;
   }
    return 0;
}

int zm_tlp_acquire_low(zm_tlp_t *L, zm_mcs_qnode_t *local_ctxt) {
    /* Acquire the low priority lock */
   zm_tlp_acquire_low_p(L, local_ctxt);
   zm_ticket_acquire(&L->filter);
   L->low_p_acq = 1;
    return 0;
}

/* Release the lock */
int zm_tlp_release(zm_tlp_t *L, zm_mcs_qnode_t* ctxt) {
   if (!L->low_p_acq) {
       if (zm_tlp_nowaiters_high_p(L, ctxt)) {
           L->go_straight = 0;
           zm_ticket_release(&L->filter);
       }
       zm_tlp_release_high_p(L, ctxt);
   } else {
       L->low_p_acq = 0;
       zm_ticket_release(&L->filter);
       zm_tlp_release_low_p(L, ctxt);
   }
    return 0;
}
