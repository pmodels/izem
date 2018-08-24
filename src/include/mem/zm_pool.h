/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef _ZM_POOL_H_
#define _ZM_POOL_H_

#include "common/zm_common.h"
#include <stdio.h>

typedef zm_ptr_t *zm_pool_t;

int zm_pool_create(size_t element_size, zm_pool_t *handle);
int zm_pool_destroy(zm_pool_t *handle);
int zm_pool_alloc(zm_pool_t handle, void **ptr);
int zm_pool_free(zm_pool_t handle, void *ptr);

#endif /* _ZM_POOL_H_ */