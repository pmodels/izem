/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "queue/zm_msqueue.h"
#include "mem/zm_hzdptr.h"

int zm_msqueue_init(zm_msqueue_t *q) {
    zm_msqnode_t* node = (zm_msqnode_t*) malloc(sizeof(zm_msqnode_t));
    node->data = NULL;
    node->next = ZM_NULL;
    atomic_store(&q->head, (zm_ptr_t)node);
    atomic_store(&q->tail, (zm_ptr_t)node);
    return 0;
}

int zm_msqueue_enqueue(zm_msqueue_t* q, void *data) {
    zm_ptr_t tail;
    zm_ptr_t next;
    zm_hzdptr_t *hzdptrs = zm_hzdptr_get();
    zm_msqnode_t* node = (zm_msqnode_t*) malloc(sizeof(zm_msqnode_t));
    node->data = data;
    atomic_store(&node->next, ZM_NULL);
    while (1) {
        tail = (zm_ptr_t) atomic_load_explicit(&q->tail, memory_order_acquire);
        hzdptrs[0] = tail;
        if(tail == atomic_load_explicit(&q->tail, memory_order_acquire)) {
            next = (zm_ptr_t) atomic_load_explicit(&((zm_msqnode_t*)tail)->next, memory_order_acquire);
            if (next == ZM_NULL) {
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

int zm_msqueue_dequeue(zm_msqueue_t* q, void **data) {
    zm_ptr_t head;
    zm_ptr_t tail;
    zm_ptr_t next;
    zm_hzdptr_t *hzdptrs = zm_hzdptr_get();
    *data = NULL;
    while (1) {
        head = (zm_ptr_t) atomic_load_explicit(&q->head, memory_order_acquire);
        hzdptrs[0] = head;
        if(head == q->head) {
            tail = (zm_ptr_t) atomic_load_explicit(&q->tail, memory_order_acquire);
            next = (zm_ptr_t) atomic_load_explicit(&((zm_msqnode_t*)head)->next, memory_order_acquire);
            hzdptrs[1] = next;
            if (head == tail) {
                if (next == ZM_NULL) return 0;
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
    zm_hzdptr_retire(head);
    return 1;
}
