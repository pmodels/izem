#ifndef _ZM_GLQUEUE_H
#define _ZM_GLQUEUE_H
#include <stdlib.h>
#include <stdio.h>
#include "queue/zm_queue_types.h"

/* glqueue: concurrent queue where both enqueue and dequeue operations
 * are protected with the same global lock (thus, the gl prefix) */

int zm_glqueue_init(zm_glqueue_t *);

static inline int zm_glqueue_enqueue(zm_glqueue_t* q, void *data) {
    /* allocate a new node */
    zm_glqnode_t* node = (zm_glqnode_t*) malloc(sizeof(zm_glqnode_t));
    /* set the data and next pointers */
    node->data = data;
    node->next = NULL;
    /* acquire the global lock */
    pthread_mutex_lock(&q->lock);
    /* chain the new node to the tail */
    ((zm_glqnode_t*)q->tail)->next = (zm_ptr_t)node;
    /* update the tail to point to the new node */
    q->tail = (zm_ptr_t)node;
    /* release the global lock */
    pthread_mutex_unlock(&q->lock);
    return 0;
}

static inline int zm_glqueue_dequeue(zm_glqueue_t* q, void **data) {
    zm_glqnode_t* head;
    /* acquire the global lock */
    pthread_mutex_lock(&q->lock);
    head = (zm_glqnode_t*)q->head;
    *data = NULL;
    /* move forward the head pointer in case the queue is not empty */
    if(head->next != NULL) {
        q->head = head->next;
         /* return the data pointer of the current head */
        *data = ((zm_glqnode_t*)q->head)->data;
        free(head);
    }
    /* release the global lock */
    pthread_mutex_unlock(&q->lock);
    return 1;
}

#endif /* _ZM_GLQUEUE_H */
