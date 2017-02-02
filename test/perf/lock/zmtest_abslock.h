#ifndef _ZMTEST_ABSLOCK_H
#define _ZMTEST_ABSLOCK_H

/* an abstraction layer for queue types and routines */

#if defined(ZMTEST_USE_MTX)
#include <pthread.h>
/* types */
#define zm_abslock_t            pthread_mutex_t
#define zm_abslock_localctx_t   int /*dummy*/
/* routines */
#define zm_abslock_init(global_lock)                    pthread_mutex_init(global_lock, NULL)
#define zm_abslock_acquire(global_lock, local_context)  pthread_mutex_lock(global_lock)
#define zm_abslock_release(global_lock, local_context)  pthread_mutex_unlock(global_lock)
#elif defined(ZMTEST_USE_TICKET)
#include <lock/zm_ticket.h>
/* types */
#define zm_abslock_t            zm_ticket_t
#define zm_abslock_localctx_t   int /*dummy*/
/* routines */
#define zm_abslock_init(global_lock)                    zm_ticket_init(global_lock)
#define zm_abslock_acquire(global_lock, local_context)  zm_ticket_acquire(global_lock)
#define zm_abslock_release(global_lock, local_context)  zm_ticket_release(global_lock)
#elif defined(ZMTEST_USE_MCS)
#include <lock/zm_mcs.h>
/* types */
#define zm_abslock_t            zm_mcs_t
#define zm_abslock_localctx_t   zm_mcs_qnode_t
/* routines */
#define zm_abslock_init                                 zm_mcs_init
#define zm_abslock_acquire(global_lock, local_context)  zm_mcs_acquire(global_lock, local_context)
#define zm_abslock_release(global_lock, local_context)  zm_mcs_release(global_lock, local_context)
#elif defined(ZMTEST_USE_MMCS)
#include <lock/zm_mmcs.h>
/* types */
#define zm_abslock_t            zm_mmcs_t
#define zm_abslock_localctx_t   zm_mcs_qnode_t
/* routines */
#define zm_abslock_init                                 zm_mmcs_init
#define zm_abslock_acquire(global_lock, local_context)  zm_mmcs_acquire(global_lock, local_context)
#define zm_abslock_release(global_lock, local_context)  zm_mmcs_release(global_lock, local_context)
#elif defined(ZMTEST_USE_TLP)
#include <lock/zm_tlp.h>
/* types */
#define zm_abslock_t            zm_tlp_t
#define zm_abslock_localctx_t   zm_mcs_qnode_t
/* routines */
#define zm_abslock_init                                 zm_tlp_init
#define zm_abslock_acquire(global_lock, local_context)  zm_tlp_acquire(global_lock, local_context)
#define zm_abslock_acquire_low(global_lock, local_context)  zm_tlp_acquire_low(global_lock, local_context)
#define zm_abslock_release(global_lock, local_context)  zm_tlp_release(global_lock, local_context)
#elif defined(ZMTEST_USE_MCSP)
#include <lock/zm_mcsp.h>
/* types */
#define zm_abslock_t            zm_mcsp_t
#define zm_abslock_localctx_t   zm_mcs_qnode_t
/* routines */
#define zm_abslock_init                                 zm_mcsp_init
#define zm_abslock_acquire(global_lock, local_context)  zm_mcsp_acquire(global_lock, local_context)
#define zm_abslock_acquire_low(global_lock, local_context)  zm_mcsp_acquire_low(global_lock, local_context)
#define zm_abslock_release(global_lock, local_context)  zm_mcsp_release(global_lock, local_context)

#elif defined(ZMTEST_USE_HMCS)
#include <lock/zm_hmcs.h>
/* types */
#define zm_abslock_t            zm_hmcs_t
#define zm_abslock_localctx_t   int /*dummy*/
/* routines */
#define zm_abslock_init                                 zm_hmcs_init
#define zm_abslock_acquire(global_lock, local_context)  zm_hmcs_acquire(*(global_lock))
#define zm_abslock_release(global_lock, local_context)  zm_hmcs_release(*(global_lock))
#else
#error "No lock implementation specified"
#endif

#endif /*_ZMTEST_ABSLOCK_H*/
