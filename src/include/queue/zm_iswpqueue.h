/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef _ZM_ISWPQUEUE_H
#define _ZM_ISWPQUEUE_H
#include <stdlib.h>
#include <stdio.h>
#include "queue/zm_queue_types.h"

ZM_INLINE_PREFIX static inline int zm_iswpqueue_init(zm_swpqueue_t *q) {
    zm_swpqnode_t* node = (zm_swpqnode_t*) malloc(sizeof(zm_swpqnode_t));
    node->data = NULL;
    node->next = ZM_NULL;
    zm_atomic_store(&q->head, (zm_ptr_t)node, zm_memord_release);
    zm_atomic_store(&q->tail, (zm_ptr_t)node, zm_memord_release);
    return 0;
}

ZM_INLINE_PREFIX static inline int zm_iswpqueue_enqueue(zm_swpqueue_t* q, void *data) {
    zm_swpqnode_t* pred;
    zm_swpqnode_t* node = (zm_swpqnode_t*) malloc(sizeof(zm_swpqnode_t));
    node->data = data;
    zm_atomic_store(&node->next, ZM_NULL, zm_memord_release);
    pred = (zm_swpqnode_t*)zm_atomic_exchange_ptr(&q->tail, (zm_ptr_t)node, zm_memord_acq_rel);
    zm_atomic_store(&pred->next, (zm_ptr_t)node, zm_memord_release);
    return 0;
}

ZM_INLINE_PREFIX static inline int zm_iswpqueue_dequeue(zm_swpqueue_t* q, void **data) {
    zm_swpqnode_t* head;
    zm_ptr_t next;
    *data = NULL;
    head = (zm_swpqnode_t*) zm_atomic_load(&q->head, zm_memord_acquire);
    /* At least one element in the queue:
            ==> head != tail
            ==> no consistency issues between enqueuers and dequeuers */
    if (head->next != ZM_NULL) {
        next = (zm_ptr_t) zm_atomic_load(&head->next, zm_memord_acquire);
        zm_atomic_store(&q->head, next, zm_memord_release);
        *data = ((zm_swpqnode_t*)next)->data;
        free(head);
    }
    return 1;
}
ZM_INLINE_PREFIX static inline int zm_iswpqueue_isempty_weak(zm_swpqueue_t* q) {
    zm_swpqnode_t* head;
    head = (zm_swpqnode_t*) zm_atomic_load(&q->head, zm_memord_acquire);
    return (head->next == ZM_NULL);
}

ZM_INLINE_PREFIX static inline int zm_iswpqueue_isempty_strong(zm_swpqueue_t* q) {
    zm_swpqnode_t* head, *tail;
    head = (zm_swpqnode_t*) zm_atomic_load(&q->head, zm_memord_acquire);
    tail = (zm_swpqnode_t*) zm_atomic_load(&q->tail, zm_memord_acquire);
    return ((head->next == ZM_NULL) && (head == tail));
}

#endif /* _ZM_ISWPQUEUE_H */
