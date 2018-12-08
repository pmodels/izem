/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef _ZM_DSM_H
#define _ZM_DSM_H
#include "common/zm_common.h"

typedef zm_ptr_t zm_dsm_t;

int zm_dsm_init(zm_dsm_t *);
int zm_dsm_sync(zm_dsm_t, void (*apply)(void *), void *);
int zm_dsm_acquire(zm_dsm_t);
int zm_dsm_release(zm_dsm_t);

#endif /* _ZM_DSM_H */
