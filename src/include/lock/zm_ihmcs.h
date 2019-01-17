/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

/*
 * The original version of this code was contributed by Milind Chabbi
 * based on the work when he was at Rice University. It relies on the
 * HMCS lock description in [1] and the fast path described in [2].
 *
 * [1] Chabbi, Milind, Michael Fagan, and John Mellor-Crummey. "High
 * performance locks for multi-level NUMA systems." In Proceedings of
 * the ACM SIGPLAN Symposium on Principles and Practice of Parallel
 * Programming (PPoPP'15), ACM, 2015.
 *
 * [2] Chabbi, Milind, and John Mellor-Crummey. "Contention-conscious,
 * locality-preserving locks." In Proceedings of the 21st ACM SIGPLAN
 * Symposium on Principles and Practice of Parallel Programming (PPoPP'16,
 * p. 22. ACM, 2016.
 */

#ifndef _ZM_IHMCS_H
#define _ZM_IHMCS_H

#include "lock/zm_lock_types.h"

#ifndef DEFAULT_THRESHOLD
#define DEFAULT_THRESHOLD 256
#endif

#ifndef HMCS_DEFAULT_MAX_LEVELS
#define HMCS_DEFAULT_MAX_LEVELS 3
#endif

#define WAIT (0xffffffff)
#define COHORT_START (0x1)
#define ACQUIRE_PARENT (0xcffffffc)

#define ZM_TRUE 1
#define ZM_FALSE 0

struct hnode{
    unsigned threshold __attribute__((aligned(ZM_CACHELINE_SIZE)));
    struct hnode * parent __attribute__((aligned(ZM_CACHELINE_SIZE)));
    zm_atomic_ptr_t lock __attribute__((aligned(ZM_CACHELINE_SIZE)));
    zm_mcs_qnode_t node __attribute__((aligned(ZM_CACHELINE_SIZE)));

}__attribute__((aligned(ZM_CACHELINE_SIZE)));

struct leaf{
    struct hnode * cur_node;
    struct hnode * root_node;
    zm_mcs_qnode_t I;
    int curDepth;
    int took_fast_path;
};

struct lock{
    // Assumes tids range from [0.. max_threads)
    // Assumes that tid 0 is close to tid and so on.
    struct leaf ** leaf_nodes __attribute__((aligned(ZM_CACHELINE_SIZE)));
    hwloc_topology_t topo;
    int levels;
};

static inline void hmcs_acquire(struct lock *) ZM_INLINE_SUFFIX;
static inline void hmcs_tryacq(struct lock *, int *) ZM_INLINE_SUFFIX;
static inline void hmcs_release(struct lock *) ZM_INLINE_SUFFIX;

int zm_ihmcs_init(zm_hmcs_t *);
int zm_ihmcs_destroy(zm_hmcs_t *);

ZM_INLINE_PREFIX static inline int zm_ihmcs_acquire(zm_hmcs_t L){
    hmcs_acquire((struct lock*)L);
    return 0;
}

ZM_INLINE_PREFIX static inline int zm_ihmcs_tryacq(zm_hmcs_t L, int *success){
    hmcs_tryacq((struct lock*)L, success);
    return 0;
}
ZM_INLINE_PREFIX static inline int zm_ihmcs_release(zm_hmcs_t L){
    hmcs_release((struct lock*)L);
    return 0;
}

/* TODO: automate hardware topology detection
 * instead of the below hard-coded method */

/* Check the actual affinity mask assigned to the thread */
ZM_INLINE_PREFIX static inline void check_affinity(hwloc_topology_t topo) {
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

ZM_INLINE_PREFIX static inline void reuse_qnode(zm_mcs_qnode_t *I){
    STORE(&I->status, WAIT);
    STORE(&I->next, ZM_NULL);
}

/* TODO: Macro or Template this for fast comprison */
ZM_INLINE_PREFIX static inline unsigned get_threshold(struct hnode *L) {
    return L->threshold;
}

ZM_INLINE_PREFIX static inline void normal_mcs_release_with_value(struct hnode * L, zm_mcs_qnode_t *I, unsigned val){

    zm_mcs_qnode_t *succ = (zm_mcs_qnode_t *)LOAD(&I->next);
    if(succ) {
        STORE(&succ->status, val);
        return;
    }
    zm_mcs_qnode_t *tmp = I;
    if (CAS(&(L->lock), (zm_ptr_t*)&tmp,ZM_NULL))
        return;
    while(succ == NULL)
        succ = (zm_mcs_qnode_t *)LOAD(&I->next); /* SPIN */
    STORE(&succ->status, val);
    return;
}

ZM_INLINE_PREFIX static inline void acquire_root(struct hnode * L, zm_mcs_qnode_t *I) {
    // Prepare the node for use.
    reuse_qnode(I);
    zm_mcs_qnode_t *pred = (zm_mcs_qnode_t*) SWAP(&(L->lock), (zm_ptr_t)I);

    if(!pred) {
        // I am the first one at this level
        return;
    }

    STORE(&pred->next, I);
    while(LOAD(&I->status) == WAIT)
        ; /* SPIN */
    return;
}

ZM_INLINE_PREFIX static inline void tryacq_root(struct hnode * L, zm_mcs_qnode_t *I, int *success) {
    zm_ptr_t expected = ZM_NULL;
    // Prepare the node for use.
    reuse_qnode(I);
    *success = CAS(&(L->lock), &expected, (zm_ptr_t)I);

    return;
}

ZM_INLINE_PREFIX static inline void release_root(struct hnode * L, zm_mcs_qnode_t *I) {
    // Top level release is usual MCS
    // At the top level MCS we always writr COHORT_START since
    // 1. It will release the lock
    // 2. Will never grow large
    // 3. Avoids a read from I->status
    normal_mcs_release_with_value(L, I, COHORT_START);
}

ZM_INLINE_PREFIX static inline void acquire_helper(int level, struct hnode * L, zm_mcs_qnode_t *I) {
    // Trivial case = root level
    if (level == 1)
        acquire_root(L, I);
    else {
        // Prepare the node for use.
        reuse_qnode(I);
        zm_mcs_qnode_t* pred = (zm_mcs_qnode_t*)SWAP(&(L->lock), (zm_ptr_t)I);
        if(!pred) {
            // I am the first one at this level
            // begining of cohort
            STORE(&I->status, COHORT_START);
            // acquire at next level if not at the top level
            acquire_helper(level - 1, L->parent, &(L->node));
            return;
        } else {
            STORE(&pred->next, I);
            for(;;){
                unsigned myStatus = LOAD(&I->status);
                if(myStatus < ACQUIRE_PARENT) {
                    return;
                }
                if(myStatus == ACQUIRE_PARENT) {
                    // beginning of cohort
                    STORE(&I->status, COHORT_START);
                    // This means this level is acquired and we can start the next level
                    acquire_helper(level - 1, L->parent, &(L->node));
                    return;
                }
                // spin back; (I->status == WAIT)
            }
        }
    }
}

ZM_INLINE_PREFIX static inline void release_helper(int level, struct hnode * L, zm_mcs_qnode_t *I) {
    // Trivial case = root level
    if (level == 1) {
        release_root(L, I);
    } else {
        unsigned cur_count = LOAD(&(I->status)) ;
        zm_mcs_qnode_t * succ;

        // Lower level releases
        if(cur_count == get_threshold(L)) {
            // NO KNOWN SUCCESSORS / DESCENDENTS
            // reached threshold and have next level
            // release to next level
            release_helper(level - 1, L->parent, &(L->node));
            //COMMIT_ALL_WRITES();
            // Tap successor at this level and ask to spin acquire next level lock
            normal_mcs_release_with_value(L, I, ACQUIRE_PARENT);
            return;
        }

        succ = (zm_mcs_qnode_t*)LOAD(&(I->next));
        // Not reached threshold
        if(succ) {
            STORE(&succ->status, cur_count + 1);
            return; // released
        }
        // No known successor, so release
        release_helper(level - 1, L->parent, &(L->node));
        // Tap successor at this level and ask to spin acquire next level lock
        normal_mcs_release_with_value(L, I, ACQUIRE_PARENT);
    }
}

ZM_INLINE_PREFIX static inline void acquire_from_leaf(int level, struct leaf *L){
    if((zm_ptr_t)L->cur_node->lock == ZM_NULL
    && (zm_ptr_t)L->root_node->lock == ZM_NULL) {
        // go FP
        L->took_fast_path = ZM_TRUE;
        acquire_root(L->root_node, &L->I);
        return;
    }
    acquire_helper(level, L->cur_node, &L->I);
    return;
}

ZM_INLINE_PREFIX static inline void tryacq_from_leaf(int level, struct leaf *L, int *success){
    *success = 0;
    if((zm_ptr_t)L->cur_node->lock == ZM_NULL
    && (zm_ptr_t)L->root_node->lock == ZM_NULL) {
        tryacq_root(L->root_node, &L->I, success);
        if (*success)
            L->took_fast_path = ZM_TRUE;
    }
    return;
}

ZM_INLINE_PREFIX static inline void release_from_leaf(int level, struct leaf *L){
    //myrelease(cur_node, I);
    if(L->took_fast_path) {
        release_root(L->root_node, &L->I);
        L->took_fast_path = ZM_FALSE;
        return;
    }
    release_helper(level, L->cur_node, &L->I);
    return;
}

ZM_INLINE_PREFIX static inline int get_hwthread_id(hwloc_topology_t topo){
    hwloc_cpuset_t cpuset = hwloc_bitmap_alloc();
    hwloc_obj_t obj;
    hwloc_get_cpubind(topo, cpuset, HWLOC_CPUBIND_THREAD);
    obj = hwloc_get_obj_inside_cpuset_by_type(topo, cpuset, HWLOC_OBJ_PU, 0);
    hwloc_bitmap_free(cpuset);
    return obj->logical_index;
}

ZM_INLINE_PREFIX static inline void hmcs_acquire(struct lock *L){
    if (zm_unlikely(tid == -1)) {
        check_affinity(L->topo);
        tid = get_hwthread_id(L->topo);
    }
    acquire_from_leaf(L->levels, L->leaf_nodes[tid]);
}

ZM_INLINE_PREFIX static inline void hmcs_tryacq(struct lock *L, int *success){
    if (zm_unlikely(tid == -1)) {
        check_affinity(L->topo);
        tid = get_hwthread_id(L->topo);
    }
    tryacq_from_leaf(L->levels, L->leaf_nodes[tid], success);
}

ZM_INLINE_PREFIX static inline void hmcs_release(struct lock *L){
    release_from_leaf(L->levels, L->leaf_nodes[tid]);
}

#endif /* _ZM_IHMCS_H */
