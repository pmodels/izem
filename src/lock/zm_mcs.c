/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdlib.h>
#include "lock/zm_mcs.h"

int zm_mcs_init(zm_mcs_t *L)
{
    atomic_store(L, (zm_ptr_t)NULL);
    return 0;
}
