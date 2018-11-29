/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef _ZM_WFQUEUE_H
#define _ZM_WFQUEUE_H
#include <stdlib.h>
#include <stdio.h>
#include "queue/zm_queue_types.h"

int zm_wfqueue_init(zm_wfqueue_t *);
int zm_wfqueue_enqueue(zm_wfqueue_t* q, void *data);
int zm_wfqueue_dequeue(zm_wfqueue_t* q, void **data);

#endif /* _ZM_WFQUEUE_H */
