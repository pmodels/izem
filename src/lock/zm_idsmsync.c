/*
  This is the implementation and optimization of DSM-Sync as described below
  according to [1]. DSM stands for distributed shared memory.
 */
#include <stdlib.h>
#include "lock/zm_idsmsync.h"

static void* new_dsm() {
    int max_threads;
    struct idsm_tnode *tnodes;


    struct dsm *D;
    posix_memalign((void **) &D, ZM_CACHELINE_SIZE, sizeof(struct dsm));

    hwloc_topology_init(&D->topo);
    hwloc_topology_load(D->topo);

    max_threads = hwloc_get_nbobjs_by_type(D->topo, HWLOC_OBJ_PU);

    posix_memalign((void **) &tnodes,
                   ZM_CACHELINE_SIZE, sizeof(struct idsm_tnode) * max_threads);
    memset(tnodes, 0, sizeof(struct idsm_tnode) * max_threads);

    STORE(&D->tail, (zm_ptr_t)ZM_NULL);
    D->local_nodes = tnodes;
    zm_imcs_init(&D->lock);

    return D;
}

static inline int free_dsm(struct dsm *D)
{
    zm_imcs_destroy(&D->lock);
    free(D->local_nodes);
    hwloc_topology_destroy(D->topo);
    free(D);
    return 0;
}

int zm_idsm_init(zm_dsm_t *handle) {
    void *p = new_dsm();
    *handle  = (zm_dsm_t) p;
    return 0;
}

int zm_idsm_destroy(zm_dsm_t *handle) {
    free_dsm((struct dsm*)(*handle));
    return 0;
}
