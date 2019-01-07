/*
  This is the implementation and optimization of DSM-Sync as described below
  according to [1]. DSM stands for distributed shared memory.

structNode {
    Request req;
    RetVal ret;
    boolean wait;
    boolean completed;
    Node *next;
};

shared Node *Tail = null;
// the following variables are private to each thread pi
private NodeMyNodesi[0..1] ={⟨⊥,⊥,FALSE,FALSE,null⟩};
private int togglei= 0;

RetVal DSM-Synch(Request req){   // pseudocode for thread pi
    Node *tmpNode, *myNode, *myPredNode;
    int counter =0;
    togglei = 1 - togglei;           // pi toggles its toggle variable
    myNode = &MyNodesi[togglei];     // pi chooses to use one of its nodes
    myNode→wait =TRUE;
    myNode→completed =FALSE;
    myNode→next =null;
    myNode→req = req;                // pi announces its request
    myPredNode = swap(Tail, myNode); // pi inserts myNode in the list
    if (myPredNode != null) {        // if a node already exists in the list
        myPredNode→next = myNode;    // fix next of previous node
        while(myNode→wait ==TRUE)    // pi spins until it is unlocked
            nop;
        if(myNode→completed ==TRUE)  // if pi’s req is already applied
            return myNode→ret;       // pi returns its return value
    }
    tmpNode = myNode;
    while (TRUE) {                   // pi is the combiner
        counter++;
        apply tmpNode→req to object’s stateand store the return value to tmpNode→ret;
        tmpNode→completed =TRUE;     // tmpNode’s req is applied
        tmpNode→wait =FALSE;         // unlock the spinning thread
        if (tmpNode→next == null or tmpNode→next→next == null or counter ≥ h)
            break;    // pi helped h threads or fewer than 2 nodes are in list
        tmpNode = tmpNode→next;      // proceed to the next node
    }
    if (tmpNode→next ==null) {       //pi’s req is the single record in list
        if(CAS(Tail, tmpNode,null)==TRUE) // try to set Tail to null
            return myNode→ret;
        while (tmpNode→next ==null)  // some thread is appending a node
            nop;                     // wait until it finishes its operation
    }
    tmpNode→next→wait =FALSE;        // unlock next node’s owner
    tmpNode→next =null;
    return myNode→ret;
}

  [1] Fatourou, Panagiota, and Nikolaos D. Kallimanis. "Revisiting the combining
   synchronization technique." In ACM SIGPLAN Notices, vol. 47, no. 8, pp.
   257-266. ACM, 2012.

 */
#include <stdlib.h>
#include "lock/zm_mcs.h"
#include "lock/zm_dsmsync.h"
#include "hwloc.h"

#define ZM_DSM_MAX_COMBINE (1 << 10)
#define ZM_UNLOCKED 0
#define ZM_WAIT 1
#define ZM_COMPLETE 2

struct dsm_qnode {
    void *req __attribute__((aligned(ZM_CACHELINE_SIZE)));
    void (*apply)(void *);
    zm_atomic_uint_t status;
    zm_atomic_ptr_t next;
};

/* thread private node */
struct dsm_tnode {
    struct dsm_qnode qnodes[2] __attribute__((aligned(ZM_CACHELINE_SIZE)));
    int toggle;
    /* to store head between combine and release operations */
    struct dsm_qnode *head __attribute__((aligned(ZM_CACHELINE_SIZE)));
};

struct dsm {
    zm_mcs_t lock;
    zm_atomic_ptr_t tail;
    struct dsm_tnode *local_nodes;
    hwloc_topology_t topo;
};

/* Check the actual affinity mask assigned to the thread */
static inline void check_affinity(hwloc_topology_t topo) {
    hwloc_cpuset_t cpuset = hwloc_bitmap_alloc();
    int set_length;
    hwloc_get_cpubind(topo, cpuset, HWLOC_CPUBIND_THREAD);
    set_length = hwloc_get_nbobjs_inside_cpuset_by_type(topo, cpuset, HWLOC_OBJ_PU);
    hwloc_bitmap_free(cpuset);

    if(set_length != 1) {
        printf("IZEM:DSM-Sync:ERROR: thread bound to more than one HW thread!\n");
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

static void* new_dsm() {
    int max_threads;
    struct dsm_tnode *tnodes;


    struct dsm *D;
    posix_memalign((void **) &D, ZM_CACHELINE_SIZE, sizeof(struct dsm));

    hwloc_topology_init(&D->topo);
    hwloc_topology_load(D->topo);

    max_threads = hwloc_get_nbobjs_by_type(D->topo, HWLOC_OBJ_PU);

    posix_memalign((void **) &tnodes,
                   ZM_CACHELINE_SIZE, sizeof(struct dsm_tnode) * max_threads);
    memset(tnodes, 0, sizeof(struct dsm_tnode) * max_threads);

    STORE(&D->tail, (zm_ptr_t)ZM_NULL);
    D->local_nodes = tnodes;
    zm_mcs_init(&D->lock);

    return D;
}

static inline int free_dsm(struct dsm *D)
{
    zm_mcs_destroy(&D->lock);
    free(D->local_nodes);
    hwloc_topology_destroy(D->topo);
    free(D);
    return 0;
}

static inline int acq_enq(struct dsm *D, struct dsm_tnode *tnode,
                          void (*apply)(void *), void *req) {
    struct dsm_qnode *local, *pred; /* "foo" = "qnode foo" */

    /* prepare my local node */
    tnode->toggle = 1 - tnode->toggle;
    local = &tnode->qnodes[tnode->toggle];
    STORE(&local->status, ZM_WAIT);
    STORE(&local->next, ZM_NULL);
    local->req = req;
    local->apply = apply;

    /* swap with globally-visible lock (queue tail)
     * this effectively announces my request "req"
     */
    pred = (struct dsm_qnode*) SWAP(&D->tail, (zm_ptr_t)local);

    /* lock owned by some other thread (combiner)
     * update "next" and wait for my status to change from WAIT
     */
    if (pred != NULL) {
        STORE(&pred->next, (zm_ptr_t)local);
        while(LOAD(&local->status) == ZM_WAIT)
            /* NOP */;
        /* if my request got completed then return */
        if(LOAD(&local->status) == ZM_COMPLETE)
            return 0;
    }

    return 0;
}

static inline int combine (struct dsm *D, struct dsm_tnode *tnode) {
    struct dsm_qnode *head, *local; /* "foo" = "qnode foo" */

    local = &tnode->qnodes[tnode->toggle];
    /* if my request got already completed, combining and releasing
     * are unnecessary. Inlining should optimize this branch out. */
    if (LOAD(&local->status) == ZM_COMPLETE) {
        tnode->head = NULL;
        return 0;
    }

    /* I am the combiner and my request is still pending */
    head = local;
    int counter = 0;
    while (1) {
        if (zm_unlikely(head->req == NULL)) {
            /* this can only mean that I am a combiner thread that
             * called "dsm_acquire" -> first loop iteration */
            assert(counter == 0);
        } else {
            head->apply(head->req);
            STORE(&head->status, ZM_COMPLETE);
        }
        if (LOAD(&head->next) == ZM_NULL ||
            LOAD(&((struct dsm_qnode*)LOAD(&head->next))->next) == ZM_NULL ||
            LOAD(&((struct dsm_qnode*)LOAD(&head->next))->req) == NULL ||
            counter > ZM_DSM_MAX_COMBINE)
            break;
        head = (struct dsm_qnode*) LOAD(&head->next);
        counter++;
    }

    tnode->head = head;

    return 0;
}

static inline int release (struct dsm *D, struct dsm_tnode *tnode) {
    struct dsm_qnode *head = tnode->head;

    /* head either points at the head of the queue or NULL if my request got
     * completed. If NULL, no need to perform release. This branch should be
     * compiled out when everything is inlined.
     */

    if (head == NULL)
        return 0;

    /* release the lock */
    if (LOAD(&head->next) == ZM_NULL) {
        zm_ptr_t expected = (zm_ptr_t)head;
        if(CAS(&D->tail, &expected, ZM_NULL))
            return 0;
        /* another thread showed up; wait for it to update next*/
        while (LOAD(&head->next) == ZM_NULL)
            /* NOP */;
    }

    /* either I reached the maximum of allowed combining operations
     * or I detected a false-positive empty queue.
     * either way, elect the next thread as the combiner.
     */
    STORE(&((struct dsm_qnode*)LOAD(&head->next))->status, ZM_UNLOCKED);
    STORE(&head->next, ZM_NULL);

    return 0;
}

static inline int dsm_sync (struct dsm *D, struct dsm_tnode *tnode,
                            void (*apply)(void *), void *req) {
    /* (1) acquire the lock or enqueue my reqeust */
    acq_enq(D, tnode, apply, req);
    /* (2) traverse the queue and comine requests if any */
    combine(D, tnode);
    /* (3) release the lock if needed. */
    release(D, tnode);

    return 0;
}

static inline int dsm_acquire (struct dsm *D, struct dsm_tnode *tnode) {
    /* (1) acquire "lock" in a traditionally mutual exclusion way */
    zm_mcs_acquire(D->lock);
    /* (2) acquire the combining queue lock */
    acq_enq(D, tnode, NULL, NULL);
    /* (3) traverse the queue and comine requests if any */
    combine(D, tnode);

    return 0;
}

static inline int dsm_release (struct dsm *D, struct dsm_tnode *tnode) {
    /* (1) release the combining queue lock */
    release(D, tnode);
    /* (2) release the mutual exclusion lock */
    zm_mcs_release(D->lock);

    return 0;
}

int zm_dsm_init(zm_dsm_t *handle) {
    void *p = new_dsm();
    *handle  = (zm_dsm_t) p;
    return 0;
}

int zm_dsm_destroy(zm_dsm_t *handle) {
    free_dsm((struct dsm*)(*handle));
    return 0;
}

int zm_dsm_sync(zm_dsm_t D, void (*apply)(void *), void *req) {
    struct dsm *d = (struct dsm*)(void *)D;
    if (zm_unlikely(tid == -1)) {
        check_affinity(d->topo);
        tid = get_hwthread_id(d->topo);
    }
    dsm_sync(d, &d->local_nodes[tid], apply, req);
    return 0;
}

int zm_dsm_acquire(zm_dsm_t D) {
    struct dsm *d = (struct dsm*)(void *)D;
    if (zm_unlikely(tid == -1)) {
        check_affinity(d->topo);
        tid = get_hwthread_id(d->topo);
    }
    dsm_acquire(d, &d->local_nodes[tid]);
    return 0;
}

int zm_dsm_release(zm_dsm_t D) {
    struct dsm *d = (struct dsm*)(void *)D;
    if (zm_unlikely(tid == -1)) {
        check_affinity(d->topo);
        tid = get_hwthread_id(d->topo);
    }
    dsm_release(d, &d->local_nodes[tid]);
    return 0;
}
