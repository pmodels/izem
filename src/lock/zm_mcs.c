/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdlib.h>
#include "lock/zm_mcs.h"

static zm_thread_local int tid = -1;

/* Check the actual affinity mask assigned to the thread */
static inline void check_affinity(hwloc_topology_t topo) {
    hwloc_cpuset_t cpuset = hwloc_bitmap_alloc();
    int set_length;
    hwloc_get_cpubind(topo, cpuset, HWLOC_CPUBIND_THREAD);
    set_length = hwloc_get_nbobjs_inside_cpuset_by_type(topo, cpuset, HWLOC_OBJ_PU);
    hwloc_bitmap_free(cpuset);

    if(set_length != 1) {
        printf("IZEM:HMCS:ERROR: thread bound to more than one HW thread!\n");
        exit(EXIT_FAILURE);
    }
}

static inline int get_hwthread_id(hwloc_topology_t topo){
    hwloc_cpuset_t cpuset = hwloc_bitmap_alloc();
    hwloc_obj_t obj;
    hwloc_get_cpubind(topo, cpuset, HWLOC_CPUBIND_THREAD);
    obj = hwloc_get_obj_inside_cpuset_by_type(topo, cpuset, HWLOC_OBJ_PU, 0);
    hwloc_bitmap_free(cpuset);
    return obj->logical_index;
}

int zm_mcs_init(zm_mcs_t *L)
{
    int max_threads;
    struct zm_mcs_qnode *qnodes;

    hwloc_topology_init(&L->topo);
    hwloc_topology_load(L->topo);

    max_threads = hwloc_get_nbobjs_by_type(L->topo, HWLOC_OBJ_PU);

    qnodes = (struct zm_mcs_qnode*) memalign(ZM_CACHELINE_SIZE, sizeof(struct zm_mcs_qnode*) * max_threads);

    zm_atomic_store(&L->lock, (zm_ptr_t)ZM_NULL, zm_memord_release);
    L->local_nodes = qnodes;

    return 0;
}

/* Main routines */
static inline int acquire_c(zm_mcs_t *L, zm_mcs_qnode_t* I) {
    zm_atomic_store(&I->next, ZM_NULL, zm_memord_release);
    zm_mcs_qnode_t* pred = (zm_mcs_qnode_t*)zm_atomic_exchange(&L->lock, (zm_ptr_t)I, zm_memord_acq_rel);
    if((zm_ptr_t)pred != ZM_NULL) {
        zm_atomic_store(&I->status, ZM_LOCKED, zm_memord_release);
        zm_atomic_store(&pred->next, (zm_ptr_t)I, zm_memord_release);
        while(zm_atomic_load(&I->status, zm_memord_acquire) != ZM_UNLOCKED)
            ; /* SPIN */
    }
    return 0;
}

/* Release the lock */
static inline int release_c(zm_mcs_t *L, zm_mcs_qnode_t *I) {
    if (zm_atomic_load(&I->next, zm_memord_acquire) == ZM_NULL) {
        zm_mcs_qnode_t *tmp = I;
        if(zm_atomic_compare_exchange_strong(&L->lock,
                                             (zm_ptr_t*)&tmp,
                                             ZM_NULL,
                                             zm_memord_acq_rel,
                                             zm_memord_acquire))
            return 0;
        while(zm_atomic_load(&I->next, zm_memord_acquire) == ZM_NULL)
            ; /* SPIN */
    }
    zm_atomic_store(&((zm_mcs_qnode_t*)zm_atomic_load(&I->next, zm_memord_acquire))->status, ZM_UNLOCKED, zm_memord_release);
    return 0;
}

static inline int nowaiters_c(zm_mcs_t *L, zm_mcs_qnode_t *I) {
    return (zm_atomic_load(&I->next, zm_memord_acquire) == ZM_NULL);
}

/* Context-less API */
int zm_mcs_acquire(zm_mcs_t *L) {
    if (zm_unlikely(tid == -1)) {
        check_affinity(L->topo);
        tid = get_hwthread_id(L->topo);
    }
    acquire_c(L, &L->local_nodes[tid]);
}

int zm_mcs_release(zm_mcs_t *L) {
    assert(tid >= 0);
    return release_c(L, &L->local_nodes[tid]);
}

int zm_mcs_nowaiters(zm_mcs_t *L) {
    assert(tid >= 0);
    return nowaiters_c(L, &L->local_nodes[tid]);
}

/* Context-full API */
int zm_mcs_acquire_c(zm_mcs_t *L, zm_mcs_qnode_t* I) {
    return acquire_c(L, I);
}

int zm_mcs_release_c(zm_mcs_t *L, zm_mcs_qnode_t *I) {
    return release_c(L, I);
}

int zm_mcs_nowaiters_c(zm_mcs_t *L, zm_mcs_qnode_t *I) {
    return nowaiters_c(L, I);
}

int zm_mcs_destroy(zm_mcs_t *L)
{
    free(L->local_nodes);
    hwloc_topology_destroy(L->topo);
    return 0;
}
