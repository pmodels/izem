/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "queue/zm_idsmqueue.h"
#include "lock/zm_idsmsync.h"

/* a queue-data pair to pass to the apply functions */
struct qdpair {
    zm_dsmqueue_t *q;
    void **data;
};

int zm_idsmqueue_init(zm_dsmqueue_t *q) {
    zm_listnode_t* node = (zm_listnode_t*) malloc(sizeof(zm_listnode_t));
    node->data = NULL;
    node->next = ZM_NULL;
    zm_idsm_init(&q->enq);
    zm_idsm_init(&q->deq);
    q->head = (zm_ptr_t)node;
    q->tail = (zm_ptr_t)node;
    return 0;
}

static inline void apply_enq(void *args) {
    struct qdpair *pair = (struct qdpair *) args;
    zm_dsmqueue_t *q = pair->q;
    void *data = *pair->data;
    zm_listnode_t* node = (zm_listnode_t*) malloc(sizeof(zm_listnode_t));
    node->data = data;
    node->next = ZM_NULL;
    ((zm_listnode_t*)q->tail)->next = (zm_ptr_t)node;
    q->tail = (zm_ptr_t)node;
}

static inline void apply_deq(void *args) {
    struct qdpair *pair = (struct qdpair *) args;
    zm_dsmqueue_t *q = pair->q;
    void **data = pair->data;
    zm_listnode_t* head;
    head = (zm_listnode_t*)q->head;
    *data = NULL;
    if(head->next != ZM_NULL) {
        q->head = head->next;
        *data = ((zm_listnode_t*)q->head)->data;
        free(head);
    }
}

int zm_idsmqueue_enqueue(zm_dsmqueue_t* q, void *data) {
    struct qdpair pair = {q, &data};
    zm_idsm_sync(q->enq, apply_enq, &pair);
    return 0;
}

int zm_idsmqueue_dequeue(zm_dsmqueue_t* q, void **data) {
    struct qdpair pair = {q, data};
    zm_idsm_sync(q->deq, apply_deq, &pair);
    return *data == NULL ? 0 : 1;
}
