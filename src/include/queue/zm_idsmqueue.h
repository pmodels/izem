/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef _ZM_IDSMQUEUE_H
#define _ZM_IDSMQUEUE_H
#include <stdlib.h>
#include <stdio.h>
#include "queue/zm_queue_types.h"

int zm_idsmqueue_init(zm_dsmqueue_t *);
int zm_idsmqueue_enqueue(zm_dsmqueue_t* q, void *data);
int zm_idsmqueue_dequeue(zm_dsmqueue_t* q, void **data);

#endif /* _ZM_IDSMQUEUE_H */
