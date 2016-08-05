#ifndef _ZMTEST_ABSQUEUE_H
#define _ZMTEST_ABSQUEUE_H

/* an abstraction layer for queue types and routines */

#if defined(ZMTEST_USE_GLQUEUE)
#include <queue/zm_glqueue.h>
/* types */
#define zm_absqueue_t       zm_glqueue_t
#define zm_absqnode_t       zm_glqnode_t
/* routines */
#define zm_absqueue_init    zm_glqueue_init
#define zm_absqueue_enqueue zm_glqueue_enqueue
#define zm_absqueue_dequeue zm_glqueue_dequeue
#elif defined(ZMTEST_USE_NMQUEUE)
#include <queue/zm_nmqueue.h>
/* types */
#define zm_absqueue_t       zm_nmqueue_t
#define zm_absqnode_t       zm_nmqnode_t
/* routines */
#define zm_absqueue_init    zm_nmqueue_init
#define zm_absqueue_enqueue zm_nmqueue_enqueue
#define zm_absqueue_dequeue zm_nmqueue_dequeue
#elif defined(ZMTEST_USE_MSQUEUE)
#include <queue/zm_msqueue.h>
/* types */
#define zm_absqueue_t       zm_msqueue_t
#define zm_absqnode_t       zm_msqnode_t
/* routines */
#define zm_absqueue_init    zm_msqueue_init
#define zm_absqueue_enqueue zm_msqueue_enqueue
#define zm_absqueue_dequeue zm_msqueue_dequeue
#else
#error "No queue implementation specified"
#endif
#endif
