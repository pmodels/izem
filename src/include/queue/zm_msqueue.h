#ifndef _ZM_MSQUEUE_H
#define _ZM_MSQUEUE_H
#include <stdlib.h>
#include <stdio.h>
#include "queue/zm_queue_types.h"

int zm_msqueue_init(zm_msqueue_t *);

static inline int zm_msqueue_enqueue(zm_msqueue_t* q, void *data) {
    zm_msqnode_t* node = (zm_msqnode_t*) malloc(sizeof(zm_msqnode_t));
    node->data = data;
    atomic_store(&node->next, NULL);
    zm_ptr_t tail;
    zm_ptr_t next;
    while (1) {
        tail = atomic_load_explicit(&q->tail, memory_order_acquire);
        next = atomic_load_explicit(&((zm_msqnode_t*)tail)->next, memory_order_acquire);
        if(tail == atomic_load_explicit(&q->tail, memory_order_acquire)) {
            if (next == NULL) {
                if (atomic_compare_exchange_weak_explicit(&((zm_msqnode_t*)tail)->next,
                                                      &next,
                                                      (zm_ptr_t)node,
                                                      memory_order_release,
                                                      memory_order_acquire))
                    break;
            } else {
                atomic_compare_exchange_weak_explicit(&q->tail,
                                                      &tail,
                                                      (zm_ptr_t)next,
                                                      memory_order_release,
                                                      memory_order_acquire);
            }
        }
    }
    atomic_compare_exchange_weak_explicit(&q->tail,
                                          &tail,
                                          (zm_ptr_t)node,
                                          memory_order_release,
                                          memory_order_acquire);
    return 0;
}

static inline int zm_msqueue_dequeue(zm_msqueue_t* q, void **data) {
    zm_ptr_t head;
    zm_ptr_t tail;
    zm_ptr_t next;
    while (1) {
        head = atomic_load_explicit(&q->head, memory_order_acquire);
        tail = atomic_load_explicit(&q->tail, memory_order_acquire);
        next = atomic_load_explicit(&((zm_msqnode_t*)head)->next, memory_order_acquire);
        if(head == q->head) {
            if (head == tail) {
                if (next == NULL) return 0;
                atomic_compare_exchange_weak_explicit(&q->tail,
                                                      &tail,
                                                      (zm_ptr_t)next,
                                                      memory_order_release,
                                                      memory_order_acquire);
            } else {
                if (atomic_compare_exchange_weak_explicit(&q->head,
                                                      &head,
                                                      (zm_ptr_t)next,
                                                      memory_order_release,
                                                      memory_order_acquire)) {
                    *data = ((zm_msqnode_t*)next)->data;
                    break;
                }
            }
        }
    }
    free(head);
    return 1;
}

#endif /* _ZM_MSQUEUE_H */
