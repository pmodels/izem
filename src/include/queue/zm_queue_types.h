#ifndef _ZM_QUEUE_TYPES_H
#define _ZM_QUEUE_TYPES_H
#include "common/zm_common.h"

typedef struct zm_msqueue zm_msqueue_t;
typedef struct zm_msqnode zm_msqnode_t;

struct zm_msqnode {
    void *data ZM_ALLIGN_TO_CACHELINE;
    zm_atomic_ptr_t next;
};

struct zm_msqueue {
    zm_atomic_ptr_t head ZM_ALLIGN_TO_CACHELINE;
    zm_atomic_ptr_t tail ZM_ALLIGN_TO_CACHELINE;
};

#endif /* _ZM_QUEUE_TYPES_H */
