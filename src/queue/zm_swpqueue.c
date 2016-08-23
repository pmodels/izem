/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "queue/zm_swpqueue.h"

int zm_swpqueue_init(zm_swpqueue_t *q) {
    zm_swpqnode_t* node = (zm_swpqnode_t*) malloc(sizeof(zm_swpqnode_t));
    node->data = NULL;
    node->next = NULL;
    atomic_store(&q->head, (zm_ptr_t)node);
    atomic_store(&q->tail, (zm_ptr_t)node);
    return 0;
}
