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

#define _GNU_SOURCE
#include <stdlib.h>
#include <assert.h>
#include <malloc.h>
#include <errno.h>
#include <pthread.h>
#include "lock/zm_lock_types.h"

#define DEFAULT_THRESHOLD 256

#define WAIT (0xffffffff)
#define COHORT_START (0x1)
#define ACQUIRE_PARENT (0xcffffffc)

#ifndef TRUE
#define TRUE 1
#else
#error "TRUE already defined"
#endif

#ifndef FALSE
#define FALSE 0
#else
#error "TRUE already defined"
#endif

/* Atomic operation shorthands. The memory ordering defaults to:
 * 1- Acquire ordering for loads
 * 2- Release ordering for stores
 * 3- Acquire+Release ordering for read-modify-write operations
 * */

#define LOAD(addr)                  zm_atomic_load(addr, zm_memord_acquire)
#define STORE(addr, val)            zm_atomic_store(addr, val, zm_memord_release)
#define SWAP(addr, desire)          zm_atomic_exchange(addr, desire, zm_memord_acq_rel)
#define CAS(addr, expect, desire)   zm_atomic_compare_exchange_strong(addr,\
                                                                      expect,\
                                                                      desire,\
                                                                      zm_memord_acq_rel,\
                                                                      zm_memord_acquire)

struct hnode{
    unsigned threshold __attribute__((aligned(ZM_CACHELINE_SIZE)));
    struct hnode * parent __attribute__((aligned(ZM_CACHELINE_SIZE)));
    zm_mcs_t lock __attribute__((aligned(ZM_CACHELINE_SIZE)));
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
};

static zm_thread_local int tid = -1;

/* TODO: automate hardware topology detection
 * instead of the below hard-coded method */

#define IT_MACHINE
//#define THING_MACHINE
//#define FIRESTONE_MACHINE
//#define LAPTOP_MACHINE

#if defined(IT_MACHINE)
static const int max_threads = 88;
static const int levels = 3;
static const int participants_at_level[] = {2,22,88};
static const int thread_mappings[] = {0, 44, 1, 45, 2, 46, 3, 47, 4, 48, 5, 49, 6, 50, 7, 51, 8, 52, 9, 53, 10, 54, 11, 55, 12, 56, 13, 57, 14, 58, 15, 59, 16, 60, 17, 61, 18, 62, 19, 63, 20, 64, 21, 65, 22, 66, 23, 67, 24, 68, 25, 69, 26, 70, 27, 71, 28, 72, 29, 73, 30, 74, 31, 75, 32, 76, 33, 77, 34, 78, 35, 79, 36, 80, 37, 81, 38, 82, 39, 83, 40, 84, 41, 85, 42, 86, 43, 87};
#elif defined(THING_MACHINE)
static const int max_threads = 72;
static const int levels = 3;
static const int participants_at_level[] = {2,18,72};
static const int thread_mappings[] = {0 , 36 , 1 , 37 , 2 , 38 , 3 , 39 , 4 , 40 , 5 , 41 , 6 , 42 , 7 , 43 , 8 , 44 , 9 , 45 , 10 , 46 , 11 , 47 , 12 , 48 , 13 , 49 , 14 , 50 , 15 , 51 , 16 , 52 , 17 , 53 , 18 , 54 , 19 , 55 , 20 , 56 , 21 , 57 , 22 , 58 , 23 , 59 , 24 , 60 , 25 , 61 , 26 , 62 , 27 , 63 , 28 , 64 , 29 , 65 , 30 , 66 , 31 , 67 , 32 , 68 , 33 , 69 , 34 , 70 , 35 , 71};
#elif defined(FIRESTONE_MACHINE)
static const int max_threads = 160;
static const int levels = 3;
static const int participants_at_level[] = {8,80,160};
static const int thread_mappings[] = {0 , 1 , 2 , 3 , 4 , 5 , 6 , 7 , 8 , 9 , 10 , 11 , 12 , 13 , 14 , 15 , 16 , 17 , 18 , 19 , 20 , 21 , 22 , 23 , 24 , 25 , 26 , 27 , 28 , 29 , 30 , 31 , 32 , 33 , 34 , 35 , 36 , 37 , 38 , 39 , 40 , 41 , 42 , 43 , 44 , 45 , 46 , 47 , 48 , 49 , 50 , 51 , 52 , 53 , 54 , 55 , 56 , 57 , 58 , 59 , 60 , 61 , 62 , 63 , 64 , 65 , 66 , 67 , 68 , 69 , 70 , 71 , 72 , 73 , 74 , 75 , 76 , 77 , 78 , 79 , 80 , 81 , 82 , 83 , 84 , 85 , 86 , 87 , 88 , 89 , 90 , 91 , 92 , 93 , 94 , 95 , 96 , 97 , 98 , 99 , 100 , 101 , 102 , 103 , 104 , 105 , 106 , 107 , 108 , 109 , 110 , 111 , 112 , 113 , 114 , 115 , 116 , 117 , 118 , 119 , 120 , 121 , 122 , 123 , 124 , 125 , 126 , 127 , 128 , 129 , 130 , 131 , 132 , 133 , 134 , 135 , 136 , 137 , 138 , 139 , 140 , 141 , 142 , 143 , 144 , 145 , 146 , 147 , 148 , 149 , 150 , 151 , 152 , 153 , 154 , 155 , 156 , 157 , 158 , 159};
#elif defined(LAPTOP_MACHINE)
static const int max_threads = 4;
static const int levels = 2;
static const int participants_at_level[] = {2,4};
static const int thread_mappings[] = {0 , 1 , 2 , 3};
#else
#error "Machine topology not recognized"
#endif

#define handle_error_en(en, msg) \
do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

static void set_affinity(int tid){
    int s, j;
    cpu_set_t cpuset;
    pthread_t thread;

    thread = pthread_self();

    /* Set affinity mask to include CPUs tid */

    CPU_ZERO(&cpuset);
    CPU_SET(1*tid, &cpuset);

    s = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (s != 0)
        handle_error_en(s, "pthread_setaffinity_np");
}

/* Check the actual affinity mask assigned to the thread */
static void check_affinity(int tid) {
    int s, j;
    cpu_set_t cpuset;
    pthread_t thread = pthread_self();
    s = pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (s != 0)
        handle_error_en(s, "pthread_getaffinity_np");

    int num_hw_threads = 0;
    for (j = 0; j < CPU_SETSIZE; j++)
        if (CPU_ISSET(j, &cpuset))
            num_hw_threads++;
    assert(num_hw_threads==1);
}

static inline void reuse_qnode(zm_mcs_qnode_t *I){
    STORE(&I->status, WAIT);
    STORE(&I->next, ZM_NULL);
}

static void* new_hnode() {
    void *storage = memalign(ZM_CACHELINE_SIZE, sizeof(struct hnode));
    if (storage == NULL) {
        printf("Memalign failed in HMCS : new_hnode \n");
        exit(EXIT_FAILURE);
    }
    return storage;
}

/* TODO: Macro or Template this for fast comprison */
static inline unsigned get_threshold(struct hnode *L) {
    return L->threshold;
}

static inline void normal_mcs_release_with_value(struct hnode * L, zm_mcs_qnode_t *I, unsigned val){

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

static inline void acquire_root(struct hnode * L, zm_mcs_qnode_t *I) {
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

static inline void release_root(struct hnode * L, zm_mcs_qnode_t *I) {
    // Top level release is usual MCS
    // At the top level MCS we always writr COHORT_START since
    // 1. It will release the lock
    // 2. Will never grow large
    // 3. Avoids a read from I->status
    normal_mcs_release_with_value(L, I, COHORT_START);
}

static inline int nowaiters_root(struct hnode * L, zm_mcs_qnode_t *I) {
    return (LOAD(&I->next) == ZM_NULL);
}

static inline void acquire_helper(int level, struct hnode * L, zm_mcs_qnode_t *I) {
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

static inline void release_helper(int level, struct hnode * L, zm_mcs_qnode_t *I) {
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

static inline int nowaiters_helper(int level, struct hnode * L, zm_mcs_qnode_t *I) {
    if (level == 1 ) {
        return nowaiters_root(L,I);
    } else {
        if(LOAD(&I->next) != ZM_NULL)
            return FALSE;
        else
            return nowaiters_helper(level - 1, L->parent, &(L->node));
    }
}

static void* new_leaf(struct hnode *h, int depth) {
    struct leaf *leaf = (struct leaf *)memalign(ZM_CACHELINE_SIZE, sizeof(struct leaf));
    if (leaf == NULL) {
        printf("Memalign failed in HMCS : new_leaf \n");
        exit(EXIT_FAILURE);
    }
    leaf->cur_node = h;
    leaf->curDepth = depth;
    leaf->took_fast_path = FALSE;
    struct hnode *tmp, *root_node;
    for(tmp = leaf->cur_node; tmp->parent != NULL; tmp = tmp->parent);
        root_node = tmp;
    leaf->root_node = root_node;
    return leaf;
}

static inline void acquire_from_leaf(struct leaf *L){
    if((zm_ptr_t)L->cur_node->lock == ZM_NULL
    && (zm_ptr_t)L->root_node->lock == ZM_NULL) {
        // go FP
        L->took_fast_path = TRUE;
        acquire_root(L->root_node, &L->I);
        return;
    }
    acquire_helper(levels, L->cur_node, &L->I);
    return;
}

static inline void release_from_leaf(struct leaf *L){
    //myrelease(cur_node, I);
    if(L->took_fast_path) {
        release_root(L->root_node, &L->I);
        L->took_fast_path = FALSE;
        return;
    }
    release_helper(levels, L->cur_node, &L->I);
    return;
}

static inline int nowaiters_from_leaf(struct leaf *L){
    // Shouldnt this be nowaiters(root_node, I)?
    if(L->took_fast_path) {
        return nowaiters_root(L->cur_node, &L->I);
    }

    return nowaiters_helper(levels, L->cur_node, &L->I);
}

static int get_hwthread_id(int id){
   for(int i = 0 ; i < max_threads; i++)
       if(id == thread_mappings[i])
            return i;

   assert(0 && "Should never reach here");
}

static void* new_lock(){

    struct lock *L = (struct lock*)memalign(ZM_CACHELINE_SIZE, sizeof(struct lock));

    // Total locks needed = participantsPerLevel[1] + participantsPerLevel[2] + .. participantsPerLevel[levels-1] + 1
    // Save affinity
    pthread_t thread = pthread_self();
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset);

    int total_locks_needed = 0;

    for (int i=0; i < levels; i++) {
        total_locks_needed += max_threads / participants_at_level[i] ;
    }
    struct hnode ** lock_locations = (struct hnode**)memalign(ZM_CACHELINE_SIZE, sizeof(struct hnode*) * total_locks_needed);
    struct leaf ** leaf_nodes = (struct leaf**)memalign(ZM_CACHELINE_SIZE, sizeof(struct leaf*) * max_threads);


    for(int tid = 0 ; tid < max_threads; tid ++){
        set_affinity(thread_mappings[tid]);
        // Pin me to hw-thread-id = tid
        int last_lock_location_end = 0;
        for(int cur_level = 0 ; cur_level < levels; cur_level++){
            if (tid%participants_at_level[cur_level] == 0) {
                // master, initialize the lock
                int lock_location = last_lock_location_end + tid/participants_at_level[cur_level];
                last_lock_location_end += max_threads/participants_at_level[cur_level];
                struct hnode * cur_hnode = new_hnode();
                cur_hnode->threshold = DEFAULT_THRESHOLD;
                cur_hnode->parent = NULL;
                cur_hnode->lock = ZM_NULL;
                lock_locations[lock_location] = cur_hnode;
            }
        }
    }

    // setup parents
    for(int tid = 0 ; tid < max_threads; tid ++){
        set_affinity(thread_mappings[tid]);
        int last_lock_location_end = 0;
        for(int cur_level = 0 ; cur_level < levels - 1; cur_level++){
            if (tid%participants_at_level[cur_level] == 0) {
                int lock_location = last_lock_location_end + tid/participants_at_level[cur_level];
                last_lock_location_end += max_threads/participants_at_level[cur_level];
                int parentLockLocation = last_lock_location_end + tid/participants_at_level[cur_level+1];
                lock_locations[lock_location]->parent = lock_locations[parentLockLocation];
            }
        }
        leaf_nodes[tid] = (struct leaf*)new_leaf(lock_locations[tid/participants_at_level[0]], levels);
    }
    free(lock_locations);
    // Restore affinity
    pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    L->leaf_nodes = leaf_nodes;

    return L;
}

static inline void hmcs_acquire(struct lock *L){
    if (zm_unlikely(tid == -1)) {
        tid = sched_getcpu();
        check_affinity(tid);
        tid = get_hwthread_id(tid);
    }
    acquire_from_leaf(L->leaf_nodes[tid]);
}

static inline void hmcs_release(struct lock *L){
    release_from_leaf(L->leaf_nodes[tid]);
}

static inline int hmcs_nowaiters(struct lock *L){
    return nowaiters_from_leaf(L->leaf_nodes[tid]);
}


int zm_hmcs_init(zm_hmcs_t * handle) {
    *handle  = (zm_hmcs_t) new_lock();
    return 0;
}

int zm_hmcs_acquire(zm_hmcs_t L){
    hmcs_acquire((struct lock*)L);
    return 0;
}
int zm_hmcs_release(zm_hmcs_t L){
    hmcs_release((struct lock*)L);
    return 0;
}
int zm_hmcs_nowaiters(zm_hmcs_t L){
    return hmcs_nowaiters((struct lock*)L);
}
