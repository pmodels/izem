/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef _ZM_IMCS_H
#define _ZM_IMCS_H
#include "common/zm_common.h"
#include "lock/zm_lock_types.h"

static inline int mcs_acquire(struct zm_mcs *);
static inline int mcs_tryacq(struct zm_mcs *, int *);
static inline int mcs_release(struct zm_mcs *);

int zm_imcs_init(zm_mcs_t *);
int zm_imcs_destroy(zm_mcs_t *);

static inline int zm_imcs_acquire(zm_mcs_t L) {
    return mcs_acquire((struct zm_mcs*)(void *)L) ;
}

static inline int zm_imcs_tryacq(zm_mcs_t L, int *success) {
    return mcs_tryacq((struct zm_mcs*)(void *)L, success) ;
}

static inline int zm_imcs_release(zm_mcs_t L) {
    return mcs_release((struct zm_mcs*)(void *)L) ;
}

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

/* Main routines */
static inline int acquire_c(struct zm_mcs *L, zm_mcs_qnode_t* I) {
    zm_atomic_store(&I->next, ZM_NULL, zm_memord_release);
    zm_mcs_qnode_t* pred = (zm_mcs_qnode_t*)zm_atomic_exchange_ptr(&L->lock, (zm_ptr_t)I, zm_memord_acq_rel);
    if((zm_ptr_t)pred != ZM_NULL) {
        zm_atomic_store(&I->status, ZM_LOCKED, zm_memord_release);
        zm_atomic_store(&pred->next, (zm_ptr_t)I, zm_memord_release);
        while(zm_atomic_load(&I->status, zm_memord_acquire) != ZM_UNLOCKED)
            ; /* SPIN */
    }
    return 0;
}

static inline int tryacq_c(struct zm_mcs *L, zm_mcs_qnode_t* I, int *success) {
    int acquired  = 0;
    zm_atomic_store(&I->next, ZM_NULL, zm_memord_release);
    zm_ptr_t expected = ZM_NULL;
    if(zm_atomic_compare_exchange_strong(&L->lock,
                                         &expected,
                                         (zm_ptr_t)I,
                                         zm_memord_acq_rel,
                                         zm_memord_acquire))
        acquired = 1;
    *success = acquired;
    return 0;
}

/* Release the lock */
static inline int release_c(struct zm_mcs *L, zm_mcs_qnode_t *I) {
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

static inline int mcs_acquire(struct zm_mcs *L) {
    if (zm_unlikely(tid == -1)) {
        check_affinity(L->topo);
        tid = get_hwthread_id(L->topo);
    }
    acquire_c(L, &L->local_nodes[tid]);
    return 0;
}

static inline int mcs_tryacq(struct zm_mcs *L, int *success) {
    if (zm_unlikely(tid == -1)) {
        check_affinity(L->topo);
        tid = get_hwthread_id(L->topo);
    }
    return tryacq_c(L, &L->local_nodes[tid], success);
}

static inline int mcs_release(struct zm_mcs *L) {
    assert(tid >= 0);
    return release_c(L, &L->local_nodes[tid]);
}

#endif /* _ZM_IMCS_H */
