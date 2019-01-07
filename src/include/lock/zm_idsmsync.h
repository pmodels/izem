/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef _ZM_IDSM_H
#define _ZM_IDSM_H
#include <hwloc.h>
#include "common/zm_common.h"
#include "lock/zm_imcs.h"

#define ZM_DSM_MAX_COMBINE (1 << 10)
#define ZM_UNLOCKED 0
#define ZM_WAIT 1
#define ZM_COMPLETE 2

typedef zm_ptr_t zm_dsm_t;

struct idsm_qnode {
    void *req __attribute__((aligned(ZM_CACHELINE_SIZE)));
    zm_atomic_uint_t status;
    zm_atomic_ptr_t next;
};

/* thread private node */
struct idsm_tnode {
    struct idsm_qnode qnodes[2] __attribute__((aligned(ZM_CACHELINE_SIZE)));
    int toggle;
    /* to store head between combine and release operations */
    struct idsm_qnode *head __attribute__((aligned(ZM_CACHELINE_SIZE)));
};

struct dsm {
    zm_mcs_t lock;
    zm_atomic_ptr_t tail;
    struct idsm_tnode *local_nodes;
    hwloc_topology_t topo;
};

static inline int acq_enq(struct dsm *D, struct idsm_tnode *tnode, void *req) {
    struct idsm_qnode *local, *pred; /* "foo" = "qnode foo" */

    /* prepare my local node */
    tnode->toggle = 1 - tnode->toggle;
    local = &tnode->qnodes[tnode->toggle];
    STORE(&local->status, ZM_WAIT);
    STORE(&local->next, ZM_NULL);
    local->req = req;

    /* swap with globally-visible lock (queue tail)
     * this effectively announces my request "req"
     */
    pred = (struct idsm_qnode*) SWAP(&D->tail, (zm_ptr_t)local);

    /* lock owned by some other thread (combiner)
     * update "next" and wait for my status to change from WAIT
     */
    if (pred != NULL) {
        STORE(&pred->next, (zm_ptr_t)local);
        while(LOAD(&local->status) == ZM_WAIT)
            /* NOP */;
        /* if my request got completed then return */
        if(LOAD(&local->status) == ZM_COMPLETE)
            return 0;
    }

    return 0;
}

#define combine(D, tnode, apply)                                            \
do {                                                                        \
    struct idsm_qnode *head, *local;                                        \
                                                                            \
    local = &tnode->qnodes[tnode->toggle];                                  \
    if (LOAD(&local->status) == ZM_COMPLETE) {                              \
        tnode->head = NULL;                                                 \
    } else {                                                                \
        head = local;                                                       \
        int counter = 0;                                                    \
        while (1) {                                                         \
            if (zm_unlikely(head->req == NULL)) {                           \
                assert(counter == 0);                                       \
            } else {                                                        \
                apply(head->req);                                           \
                STORE(&head->status, ZM_COMPLETE);                          \
            }                                                               \
            if (LOAD(&head->next) == ZM_NULL ||                             \
                LOAD(&((struct idsm_qnode*)LOAD(&head->next))->next) == ZM_NULL || \
                LOAD(&((struct idsm_qnode*)LOAD(&head->next))->req) == NULL || \
                counter > ZM_DSM_MAX_COMBINE)                               \
                break;                                                      \
            head = (struct idsm_qnode*) LOAD(&head->next);                  \
            counter++;                                                      \
        }                                                                   \
        tnode->head = head;                                                 \
    }                                                                       \
} while (0)

static inline int release (struct dsm *D, struct idsm_tnode *tnode) {
    struct idsm_qnode *head = tnode->head;

    /* head either points at the head of the queue or NULL if my request got
     * completed. If NULL, no need to perform release. This branch should be
     * compiled out when everything is inlined.
     */

    if (head == NULL)
        return 0;

    /* release the lock */
    if (LOAD(&head->next) == ZM_NULL) {
        zm_ptr_t expected = (zm_ptr_t)head;
        if(CAS(&D->tail, &expected, ZM_NULL))
            return 0;
        /* another thread showed up; wait for it to update next*/
        while (LOAD(&head->next) == ZM_NULL)
            /* NOP */;
    }

    /* either I reached the maximum of allowed combining operations
     * or I detected a false-positive empty queue.
     * either way, elect the next thread as the combiner.
     */
    STORE(&((struct idsm_qnode*)LOAD(&head->next))->status, ZM_UNLOCKED);
    STORE(&head->next, ZM_NULL);

    return 0;
}

#define dsm_sync(D, tnode, apply, req)                                      \
do {                                                                        \
    acq_enq(D, tnode, req);                                                 \
    combine(D, tnode, apply);                                               \
    release(D, tnode);                                                      \
} while (0)

#define dsm_acquire(D, tnode, apply)                                        \
do {                                                                        \
    zm_imcs_acquire(D->lock);                                               \
    acq_enq(D, tnode, NULL);                                                \
    combine(D, tnode, apply);                                               \
} while (0)

static inline int dsm_release (struct dsm *D, struct idsm_tnode *tnode) {
    /* (1) release the combining queue lock */
    release(D, tnode);
    /* (2) release the mutual exclusion lock */
    zm_imcs_release(D->lock);

    return 0;
}

int zm_idsm_init(zm_dsm_t *);
int zm_idsm_destroy(zm_dsm_t *);

#define zm_idsm_sync(D, apply, req)                                         \
do {                                                                        \
    struct dsm *d = (struct dsm*)(void *)(D);                               \
    if (zm_unlikely(tid == -1)) {                                           \
        check_affinity(d->topo);                                            \
        tid = get_hwthread_id(d->topo);                                     \
    }                                                                       \
    dsm_sync(d, (&d->local_nodes[tid]), apply, req);                        \
} while (0)

#define zm_idsm_acquire(D, apply)                                           \
do {                                                                        \
    struct dsm *d = (struct dsm*)(void *)D;                                 \
    if (zm_unlikely(tid == -1)) {                                           \
        check_affinity(d->topo);                                            \
        tid = get_hwthread_id(d->topo);                                     \
    }                                                                       \
    dsm_acquire(d, (&d->local_nodes[tid]), apply);                          \
} while (0)

static inline int zm_idsm_release(zm_dsm_t D) {
    struct dsm *d = (struct dsm*)(void *)D;
    if (zm_unlikely(tid == -1)) {
        check_affinity(d->topo);
        tid = get_hwthread_id(d->topo);
    }
    dsm_release(d, &d->local_nodes[tid]);
    return 0;
}

#endif /* _ZM_IDSM_H */
