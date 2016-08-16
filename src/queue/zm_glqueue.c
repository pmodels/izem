/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "queue/zm_glqueue.h"

int zm_glqueue_init(zm_glqueue_t *q) {
    zm_glqnode_t* node = (zm_glqnode_t*) malloc(sizeof(zm_glqnode_t));
    node->data = NULL;
    node->next = NULL;
    pthread_mutex_init(&q->lock, NULL);
    q->head = (zm_ptr_t)node;
    q->tail = (zm_ptr_t)node;
    return 0;
}
