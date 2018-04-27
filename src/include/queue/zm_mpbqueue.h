/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef _ZM_MPBQUEUE_H
#define _ZM_MPBQUEUE_H
#include <stdlib.h>
#include <stdio.h>
#include "queue/zm_queue_types.h"

/* mpbqueue: MPB: Multiple Producer Bucket queue. Concurrent queue where both enqueue and dequeue operations
 * are protected with the same global lock (thus, the gl prefix) */

int zm_mpbqueue_init(zm_mpbqueue_t *, int);
int zm_mpbqueue_enqueue(zm_mpbqueue_t* q, void *data, int);
int zm_mpbqueue_dequeue(zm_mpbqueue_t* q, void **data, int);

#endif /* _ZM_MPBQUEUE_H */
