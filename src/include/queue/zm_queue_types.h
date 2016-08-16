/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef _ZM_QUEUE_TYPES_H
#define _ZM_QUEUE_TYPES_H
#include "common/zm_common.h"
#include <pthread.h>

/* glqueue*/
typedef struct zm_glqueue zm_glqueue_t;
typedef struct zm_glqnode zm_glqnode_t;

struct zm_glqnode {
    void *data ZM_ALLIGN_TO_CACHELINE;
    zm_ptr_t next;
};

struct zm_glqueue {
    pthread_mutex_t lock;
    zm_ptr_t head ZM_ALLIGN_TO_CACHELINE;
    zm_ptr_t tail ZM_ALLIGN_TO_CACHELINE;
};

/* nmqueue */
typedef struct zm_msqueue zm_nmqueue_t;
typedef struct zm_msqnode zm_nmqnode_t;

/* msqueue */
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
