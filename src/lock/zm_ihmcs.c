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

#include "lock/zm_ihmcs.h"

static void* new_hnode() {
    int err;
    void *storage;
    err = posix_memalign(&storage, ZM_CACHELINE_SIZE, sizeof(struct hnode));
    if (err != 0) {
        printf("posix_memalign failed in HMCS : new_hnode \n");
        exit(EXIT_FAILURE);
    }
    return storage;
}

static void* new_leaf(struct hnode *h, int depth) {
    int err;
    struct leaf *leaf;
    err = posix_memalign((void **) &leaf, ZM_CACHELINE_SIZE, sizeof(struct leaf));
    if (err != 0) {
        printf("posix_memalign failed in HMCS : new_leaf \n");
        exit(EXIT_FAILURE);
    }
    leaf->cur_node = h;
    leaf->curDepth = depth;
    leaf->took_fast_path = ZM_FALSE;
    struct hnode *tmp, *root_node;
    for(tmp = leaf->cur_node; tmp->parent != NULL; tmp = tmp->parent);
    root_node = tmp;
    leaf->root_node = root_node;
    return leaf;
}

static void set_hierarchy(struct lock *L, int *max_threads, int** particip_per_level) {
    int max_depth, levels = 0, max_levels = HMCS_DEFAULT_MAX_LEVELS, explicit_levels = 0;
    char tmp[20];
    char *s = getenv("ZM_HMCS_MAX_LEVELS");
    if (s != NULL)
        max_levels = atoi(s);
    int depths[max_levels];
    int idx = 0;
    /* advice to users: run `hwloc-ls -s --no-io --no-icaches` and choose
     * depths of interest in ascending order. The first depth must be `0' */

    s = getenv("ZM_HMCS_EXPLICIT_LEVELS");
    if (s != NULL) {
        strcpy(tmp, s);
        explicit_levels = 1;
        char* token;
        token = strtok(tmp,",");
        while(token != NULL) {
            depths[idx] = atoi(token);
            if (idx == 0)
                assert(depths[idx] == 0 && "the first depth must be machine level (i.e., depth 0), run `hwloc-ls -s --no-io --no-icaches` and choose appropriate depth values");
            idx++;
            token = strtok(NULL,",");
        }
        assert(idx == max_levels);
    }

    hwloc_topology_init(&L->topo);
    hwloc_topology_load(L->topo);

    *max_threads = hwloc_get_nbobjs_by_type(L->topo, HWLOC_OBJ_PU);

    max_depth = hwloc_topology_get_depth(L->topo);
    assert(max_depth >= 2); /* At least Machine and Core levels exist */

    *particip_per_level = (int*) malloc(max_levels * sizeof(int));
    int prev_nobjs = -1;
    if(!explicit_levels) {
        for (int d = max_depth - 2; d > 1; d--) {
            int cur_nobjs = hwloc_get_nbobjs_by_depth(L->topo, d);
            /* Check if this level has a hierarchical impact */
            if(cur_nobjs != prev_nobjs) {
                prev_nobjs = cur_nobjs;
                (*particip_per_level)[levels] = (*max_threads)/cur_nobjs;
                levels++;
                if(levels >= max_levels - 1)
                    break;
            }
        }
        (*particip_per_level)[levels] = *max_threads;
        levels++;
    } else {
        for(int i = max_levels - 1; i >= 0; i--){
            int d = depths[i];
            int cur_nobjs = hwloc_get_nbobjs_by_depth(L->topo, d);
            /* Check if this level has a hierarchical impact */
            if(cur_nobjs != prev_nobjs) {
                prev_nobjs = cur_nobjs;
                (*particip_per_level)[levels] = (*max_threads)/cur_nobjs;
                levels++;
            } else {
                assert(0 && "plz choose levels that have a hierarchical impact");
            }
        }
    }

    L->levels = levels;
}

static void free_hierarchy(int* particip_per_level){
    free(particip_per_level);
}

static void* new_lock(){

    struct lock *L;
    posix_memalign((void **) &L, ZM_CACHELINE_SIZE, sizeof(struct lock));

    int max_threads;
    int *participants_at_level;
    set_hierarchy(L, &max_threads, &participants_at_level);

    // Total locks needed = participantsPerLevel[1] + participantsPerLevel[2] + .. participantsPerLevel[levels-1] + 1
    // Save affinity
    hwloc_cpuset_t cpuset = hwloc_bitmap_alloc();
    hwloc_get_cpubind(L->topo, cpuset, HWLOC_CPUBIND_THREAD);

    int total_locks_needed = 0;
    int levels = L->levels;

    for (int i=0; i < levels; i++) {
        total_locks_needed += max_threads / participants_at_level[i] ;
    }
    struct hnode ** lock_locations;
    posix_memalign((void **) &lock_locations, ZM_CACHELINE_SIZE, sizeof(struct hnode*) * total_locks_needed);
    struct leaf ** leaf_nodes;
    posix_memalign((void **) &leaf_nodes, ZM_CACHELINE_SIZE, sizeof(struct leaf*) * max_threads);

    int threshold = DEFAULT_THRESHOLD;
    char *s = getenv("ZM_HMCS_THRESHOLD");
    if (s != NULL)
        threshold = atoi(s);

    hwloc_obj_t obj;
    for(int thid = 0 ; thid < max_threads; thid ++){
        obj = hwloc_get_obj_by_type (L->topo, HWLOC_OBJ_PU, thid);
        hwloc_set_cpubind(L->topo, obj->cpuset, HWLOC_CPUBIND_THREAD);
        // Pin me to hw-thread-id = thid
        int last_lock_location_end = 0;
        for(int cur_level = 0 ; cur_level < levels; cur_level++){
            if (thid%participants_at_level[cur_level] == 0) {
                // master, initialize the lock
                int lock_location = last_lock_location_end + thid/participants_at_level[cur_level];
                last_lock_location_end += max_threads/participants_at_level[cur_level];
                struct hnode * cur_hnode = new_hnode();
                cur_hnode->threshold = threshold;
                cur_hnode->parent = NULL;
                cur_hnode->lock = ZM_NULL;
                lock_locations[lock_location] = cur_hnode;
            }
        }
    }

    // setup parents
    for(int thid = 0 ; thid < max_threads; thid ++){
        obj = hwloc_get_obj_by_type (L->topo, HWLOC_OBJ_PU, thid);
        hwloc_set_cpubind(L->topo, obj->cpuset, HWLOC_CPUBIND_THREAD);
        int last_lock_location_end = 0;
        for(int cur_level = 0 ; cur_level < levels - 1; cur_level++){
            if (thid%participants_at_level[cur_level] == 0) {
                int lock_location = last_lock_location_end + thid/participants_at_level[cur_level];
                last_lock_location_end += max_threads/participants_at_level[cur_level];
                int parentLockLocation = last_lock_location_end + thid/participants_at_level[cur_level+1];
                lock_locations[lock_location]->parent = lock_locations[parentLockLocation];
            }
        }
        leaf_nodes[thid] = (struct leaf*)new_leaf(lock_locations[thid/participants_at_level[0]], levels);
    }
    free(lock_locations);
    free_hierarchy(participants_at_level);
    // Restore affinity
    hwloc_set_cpubind(L->topo, cpuset, HWLOC_CPUBIND_THREAD);
    L->leaf_nodes = leaf_nodes;

    hwloc_bitmap_free(cpuset);

    return L;
}

static void search_nodes_rec(struct hnode *node, struct hnode **nodes_to_free, int *num_ptrs, int max_threads) {
    int i;
    if(node != NULL) {
        for(i = 0; i < *num_ptrs; i++) {
            if(node == nodes_to_free[i])
                break; /* already marked to be free'd */
        }
        if(i == *num_ptrs) { /* newly encountered pointer */
            nodes_to_free[*num_ptrs] = node;
            (*num_ptrs)++;
            assert(*num_ptrs < 2*max_threads);
        }
        search_nodes_rec(node->parent, nodes_to_free, num_ptrs, max_threads);
    }
}

static void free_lock(struct lock* L) {
    int max_threads = hwloc_get_nbobjs_by_type(L->topo, HWLOC_OBJ_PU);
    int num_ptrs = 0;
    struct hnode **nodes_to_free = (struct hnode**) malloc(2*max_threads*sizeof(struct hnode*));
    for (int thid = 0; thid < max_threads; thid++) {
        search_nodes_rec(L->leaf_nodes[thid]->cur_node, nodes_to_free, &num_ptrs, max_threads);
        free(L->leaf_nodes[thid]);
    }
    free(L->leaf_nodes);
    for(int i = 0; i < num_ptrs; i++)
        free(nodes_to_free[i]);
    free(nodes_to_free);
    hwloc_topology_destroy(L->topo);
    free(L);
}

int zm_ihmcs_init(zm_hmcs_t * handle) {
    void *p = new_lock();
    *handle  = (zm_hmcs_t) p;
    return 0;
}

int zm_ihmcs_destroy(zm_hmcs_t *L) {
    free_lock((struct lock*)(*L));
    return 0;
}
