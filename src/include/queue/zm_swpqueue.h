/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef _ZM_SWPQUEUE_H
#define _ZM_SWPQUEUE_H
#include <stdlib.h>
#include <stdio.h>
#include "queue/zm_queue_types.h"
#include "mem/zm_hzdptr.h"

int zm_swpqueue_init(zm_swpqueue_t *);

static inline int zm_swpqueue_enqueue(zm_swpqueue_t* q, void *data) {
    zm_swpqnode_t* pred;
    zm_swpqnode_t* node = (zm_swpqnode_t*) malloc(sizeof(zm_swpqnode_t));
    node->data = data;
    atomic_store_explicit(&node->next, NULL, memory_order_release);
    pred = (zm_swpqnode_t*)atomic_exchange_explicit(&q->tail, (zm_ptr_t)node, memory_order_acq_rel);
    atomic_store_explicit(&pred->next, (zm_ptr_t)node, memory_order_release);
    return 0;
}

static inline int zm_swpqueue_dequeue(zm_swpqueue_t* q, void **data) {
    zm_swpqnode_t* head;
    zm_ptr_t next;
    *data = NULL;
    head = (zm_swpqnode_t*) atomic_load_explicit(&q->head, memory_order_acquire);
    /* At least one element in the queue: 
            ==> head != tail 
            ==> no consistency issues between enqueuers and dequeuers */
    if (head->next != NULL) {
        next = (zm_ptr_t) atomic_load_explicit(&head->next, memory_order_acquire);
        atomic_store_explicit(&q->head, next, memory_order_release);
        *data = ((zm_swpqnode_t*)next)->data;
        free(head);
    }
    return 1;
}

#endif /* _ZM_SWPQUEUE_H */
