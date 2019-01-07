/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdlib.h>
#include <hwloc.h>
#include "lock/zm_imcs.h"

static void* new_lock() {
    int max_threads;
    struct zm_mcs_qnode *qnodes;


    struct zm_mcs *L;
    posix_memalign((void **) &L, ZM_CACHELINE_SIZE, sizeof(struct zm_mcs));

    hwloc_topology_init(&L->topo);
    hwloc_topology_load(L->topo);

    max_threads = hwloc_get_nbobjs_by_type(L->topo, HWLOC_OBJ_PU);

    posix_memalign((void **) &qnodes, ZM_CACHELINE_SIZE, sizeof(struct zm_mcs_qnode) * max_threads);

    zm_atomic_store(&L->lock, (zm_ptr_t)ZM_NULL, zm_memord_release);
    L->local_nodes = qnodes;

    return L;
}

static inline int free_lock(struct zm_mcs *L)
{
    free(L->local_nodes);
    hwloc_topology_destroy(L->topo);
    return 0;
}

int zm_imcs_init(zm_mcs_t *handle) {
    void *p = new_lock();
    *handle  = (zm_mcs_t) p;
    return 0;
}

int zm_imcs_destroy(zm_mcs_t *L) {
    free_lock((struct zm_mcs*)(*L));
    return 0;
}
