/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef _ZM_SWPQUEUE_H
#define _ZM_SWPQUEUE_H
#include <stdlib.h>
#include <stdio.h>
#include "queue/zm_queue_types.h"

int zm_swpqueue_init(zm_swpqueue_t *);
int zm_swpqueue_enqueue(zm_swpqueue_t* q, void *data);
int zm_swpqueue_dequeue(zm_swpqueue_t* q, void **data);
int zm_swpqueue_isempty_weak(zm_swpqueue_t* q);
int zm_swpqueue_isempty_strong(zm_swpqueue_t* q);

int zm_swpqueue_init_explicit(zm_swpqueue_t *, int);
int zm_swpqueue_enqueue_explicit(zm_swpqueue_t* q, void *data, int);
int zm_swpqueue_dequeue_explicit(zm_swpqueue_t* q, void **data, int);


#endif /* _ZM_SWPQUEUE_H */
