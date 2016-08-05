#include "queue/zm_nmqueue.h"

int zm_nmqueue_init(zm_nmqueue_t *q) {
    zm_nmqnode_t* node = (zm_nmqnode_t*) malloc(sizeof(zm_nmqnode_t));
    node->data = NULL;
    node->next = NULL;
    atomic_store(&q->head, (zm_ptr_t)node);
    atomic_store(&q->tail, (zm_ptr_t)node);
    return 0;
}
