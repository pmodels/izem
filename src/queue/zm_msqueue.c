#include "queue/zm_msqueue.h"

int zm_msqueue_init(zm_msqueue_t *q) {
    zm_msqnode_t* node = (zm_msqnode_t*) malloc(sizeof(zm_msqnode_t));
    node->data = NULL;
    node->next = NULL;
    atomic_store(&q->head, (zm_ptr_t)node);
    atomic_store(&q->tail, (zm_ptr_t)node);
    return 0;
}
