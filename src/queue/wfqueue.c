/* The MIT License (MIT)

Copyright (c) 2015 Chaoran Yang

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE. */

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <hwloc.h>
#include "queue/zm_wfqueue.h"

/* From align.h */
#define PAGE_SIZE 4096
#define CACHE_LINE_SIZE 64
#define CACHE_ALIGNED __attribute__((aligned(CACHE_LINE_SIZE)))
#define DOUBLE_CACHE_ALIGNED __attribute__((aligned(2 * CACHE_LINE_SIZE)))

static inline void * align_malloc(size_t align, size_t size)
{
  void * ptr;

  int ret = posix_memalign(&ptr, align, size);
  if (ret != 0) {
    fprintf(stderr, strerror(ret));
    abort();
  }

  return ptr;
}

/* From wfqueue.h */
#define EMPTY ((void *) 0)

#ifndef WFQUEUE_NODE_SIZE
#define WFQUEUE_NODE_SIZE ((1 << 10) - 2)
#endif

struct _enq_t {
  long volatile id;
  void * volatile val;
} CACHE_ALIGNED;

struct _deq_t {
  long volatile id;
  long volatile idx;
} CACHE_ALIGNED;

struct _cell_t {
  void * volatile val;
  struct _enq_t * volatile enq;
  struct _deq_t * volatile deq;
  void * pad[5];
};

struct _node_t {
  struct _node_t * volatile next CACHE_ALIGNED;
  long id CACHE_ALIGNED;
  struct _cell_t cells[WFQUEUE_NODE_SIZE] CACHE_ALIGNED;
};


typedef struct _handle_t handle_t;

typedef struct DOUBLE_CACHE_ALIGNED {
  /**
   * Index of the next position for enqueue.
   */
  volatile long Ei DOUBLE_CACHE_ALIGNED;

  /**
   * Index of the next position for dequeue.
   */
  volatile long Di DOUBLE_CACHE_ALIGNED;

  /**
   * Index of the head of the queue.
   */
  volatile long Hi DOUBLE_CACHE_ALIGNED;

  /**
   * Pointer to the head node of the queue.
   */
  struct _node_t * volatile Hp;

  /**
   * Number of processors.
   */
  long nprocs;

  hwloc_topology_t topo;
  handle_t *handles;
#ifdef RECORD
  long slowenq;
  long slowdeq;
  long fastenq;
  long fastdeq;
  long empty;
#endif
} queue_t;

struct _handle_t {
  /**
   * Pointer to the next handle.
   */
  struct _handle_t * next;

  /**
   * Hazard pointer.
   */
  //struct _node_t * volatile Hp;
  unsigned long volatile hzd_node_id;

  /**
   * Pointer to the node for enqueue.
   */
  struct _node_t * volatile Ep;
  unsigned long enq_node_id;

  /**
   * Pointer to the node for dequeue.
   */
  struct _node_t * volatile Dp;
  unsigned long deq_node_id;

  /**
   * Enqueue request.
   */
  struct _enq_t Er CACHE_ALIGNED;

  /**
   * Dequeue request.
   */
  struct _deq_t Dr CACHE_ALIGNED;

  /**
   * Handle of the next enqueuer to help.
   */
  struct _handle_t * Eh CACHE_ALIGNED;

  long Ei;

  /**
   * Handle of the next dequeuer to help.
   */
  struct _handle_t * Dh;

  /**
   * Pointer to a spare node to use, to speedup adding a new node.
   */
  struct _node_t * spare CACHE_ALIGNED;

  /**
   * Count the delay rounds of helping another dequeuer.
   */
  int delay;

#ifdef RECORD
  long slowenq;
  long slowdeq;
  long fastenq;
  long fastdeq;
  long empty;
#endif
};

/* From: primitives.h */

#if defined(__GNUC__) && __GNUC__ >= 4 && __GNUC_MINOR__ > 7
/**
 * An atomic fetch-and-add.
 */
#define FAA(ptr, val) __atomic_fetch_add(ptr, val, __ATOMIC_RELAXED)
/**
 * An atomic fetch-and-add that also ensures sequential consistency.
 */
#define FAAcs(ptr, val) __atomic_fetch_add(ptr, val, __ATOMIC_SEQ_CST)

/**
 * An atomic compare-and-swap.
 */
#define CAS(ptr, cmp, val) __atomic_compare_exchange_n(ptr, cmp, val, 0, \
    __ATOMIC_RELAXED, __ATOMIC_RELAXED)
/**
 * An atomic compare-and-swap that also ensures sequential consistency.
 */
#define CAScs(ptr, cmp, val) __atomic_compare_exchange_n(ptr, cmp, val, 0, \
    __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)
/**
 * An atomic compare-and-swap that ensures release semantic when succeed
 * or acquire semantic when failed.
 */
#define CASra(ptr, cmp, val) __atomic_compare_exchange_n(ptr, cmp, val, 0, \
    __ATOMIC_RELEASE, __ATOMIC_ACQUIRE)
/**
 * An atomic compare-and-swap that ensures acquire semantic when succeed
 * or relaxed semantic when failed.
 */
#define CASa(ptr, cmp, val) __atomic_compare_exchange_n(ptr, cmp, val, 0, \
    __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)

/**
 * An atomic swap.
 */
#define SWAP(ptr, val) __atomic_exchange_n(ptr, val, __ATOMIC_RELAXED)

/**
 * An atomic swap that ensures acquire release semantics.
 */
#define SWAPra(ptr, val) __atomic_exchange_n(ptr, val, __ATOMIC_ACQ_REL)

/**
 * A memory fence to ensure sequential consistency.
 */
#define FENCE() __atomic_thread_fence(__ATOMIC_SEQ_CST)

/**
 * An atomic store.
 */
#define STORE(ptr, val) __atomic_store_n(ptr, val, __ATOMIC_RELAXED)

/**
 * A store with a preceding release fence to ensure all previous load
 * and stores completes before the current store is visiable.
 */
#define RELEASE(ptr, val) __atomic_store_n(ptr, val, __ATOMIC_RELEASE)

/**
 * A load with a following acquire fence to ensure no following load and
 * stores can start before the current load completes.
 */
#define ACQUIRE(ptr) __atomic_load_n(ptr, __ATOMIC_ACQUIRE)

#else /** Non-GCC or old GCC. */
#if defined(__x86_64__) || defined(_M_X64_)

#define FAA __sync_fetch_and_add
#define FAAcs __sync_fetch_and_add

static inline int
_compare_and_swap(void ** ptr, void ** expected, void * desired) {
  void * oldval = *expected;
  void * newval = __sync_val_compare_and_swap(ptr, oldval, desired);

  if (newval == oldval) {
    return 1;
  } else {
    *expected = newval;
    return 0;
  }
}
#define CAS(ptr, expected, desired) \
  _compare_and_swap((void **) (ptr), (void **) (expected), (void *) (desired))
#define CAScs CAS
#define CASra CAS
#define CASa  CAS

#define SWAP __sync_lock_test_and_set
#define SWAPra SWAP

#define ACQUIRE(p) ({ \
  __typeof__(*(p)) __ret = *p; \
  __asm__("":::"memory"); \
  __ret; \
})

#define RELEASE(p, v) do {\
  __asm__("":::"memory"); \
  *p = v; \
} while (0)
#define FENCE() __sync_synchronize()

#endif
#endif

#if defined(__x86_64__) || defined(_M_X64_)
#define PAUSE() __asm__ ("pause")

static inline
int _CAS2(volatile long * ptr, long * cmp1, long * cmp2, long val1, long val2)
{
  char success;
  long tmp1 = *cmp1;
  long tmp2 = *cmp2;

  __asm__ __volatile__(
      "lock cmpxchg16b %1\n"
      "setz %0"
      : "=q" (success), "+m" (*ptr), "+a" (tmp1), "+d" (tmp2)
      : "b" (val1), "c" (val2)
      : "cc" );

  *cmp1 = tmp1;
  *cmp2 = tmp2;
  return success;
}
#define CAS2(p, o1, o2, n1, n2) \
  _CAS2((volatile long *) p, (long *) o1, (long *) o2, (long) n1, (long) n2)

#define BTAS(ptr, bit) ({ \
  char __ret; \
  __asm__ __volatile__( \
      "lock btsq %2, %0; setnc %1" \
      : "+m" (*ptr), "=r" (__ret) : "ri" (bit) : "cc" ); \
  __ret; \
})

#else
#define PAUSE()
#endif


#define N WFQUEUE_NODE_SIZE
#define BOT ((void *)0)
#define TOP ((void *)-1)

#define MAX_GARBAGE(n) (2 * n)

#ifndef MAX_SPIN
#define MAX_SPIN 100
#endif

#ifndef MAX_PATIENCE
#define MAX_PATIENCE 10
#endif

typedef struct _enq_t enq_t;
typedef struct _deq_t deq_t;
typedef struct _cell_t cell_t;
typedef struct _node_t node_t;

static inline void *spin(void *volatile *p) {
    int patience = MAX_SPIN;
    void *v = *p;

    while (!v && patience-- > 0) {
        v = *p;
        PAUSE();
    }

    return v;
}

static inline node_t *new_node() {
    node_t *n = align_malloc(PAGE_SIZE, sizeof(node_t));
    memset(n, 0, sizeof(node_t));
    return n;
}

static node_t *check(unsigned long volatile *p_hzd_node_id, node_t *cur,
                     node_t *old) {
    unsigned long hzd_node_id = ACQUIRE(p_hzd_node_id);

    if (hzd_node_id < cur->id) {
        node_t *tmp = old;
        while (tmp->id < hzd_node_id) {
            tmp = tmp->next;
        }
        cur = tmp;
    }

    return cur;
}

static node_t *update(node_t *volatile *pPn, node_t *cur,
                      unsigned long volatile *p_hzd_node_id, node_t *old) {
    node_t *ptr = ACQUIRE(pPn);

    if (ptr->id < cur->id) {
        if (!CAScs(pPn, &ptr, cur)) {
            if (ptr->id < cur->id) cur = ptr;
        }

        cur = check(p_hzd_node_id, cur, old);
    }

    return cur;
}

static void cleanup(queue_t *q, handle_t *th) {
    long oid = ACQUIRE(&q->Hi);
    node_t *new = th->Dp;

    if (oid == -1) return;
    if (new->id - oid < MAX_GARBAGE(q->nprocs)) return;
    if (!CASa(&q->Hi, &oid, -1)) return;

    node_t *old = q->Hp;
    handle_t *ph = th;
    handle_t *phs[q->nprocs];
    int i = 0;

    do {
        new = check(&ph->hzd_node_id, new, old);
        new = update(&ph->Ep, new, &ph->hzd_node_id, old);
        new = update(&ph->Dp, new, &ph->hzd_node_id, old);

        phs[i++] = ph;
        ph = ph->next;
    } while (new->id > oid && ph != th);

    while (new->id > oid && --i >= 0) {
        new = check(&phs[i]->hzd_node_id, new, old);
    }

    long nid = new->id;

    if (nid <= oid) {
        RELEASE(&q->Hi, oid);
    } else {
        q->Hp = new;
        RELEASE(&q->Hi, nid);

        while (old != new) {
            node_t *tmp = old->next;
            free(old);
            old = tmp;
        }
    }
}

static cell_t *find_cell(node_t *volatile *ptr, long i, handle_t *th) {
    node_t *curr = *ptr;

    long j;
    for (j = curr->id; j < i / N; ++j) {
        node_t *next = curr->next;

        if (next == NULL) {
            node_t *temp = th->spare;

            if (!temp) {
                temp = new_node();
                th->spare = temp;
            }

            temp->id = j + 1;

            if (CASra(&curr->next, &next, temp)) {
                next = temp;
                th->spare = NULL;
            }
        }

        curr = next;
    }

    *ptr = curr;
    return &curr->cells[i % N];
}

static int enq_fast(queue_t *q, handle_t *th, void *v, long *id) {
    long i = FAAcs(&q->Ei, 1);
    cell_t *c = find_cell(&th->Ep, i, th);
    void *cv = BOT;

    if (CAS(&c->val, &cv, v)) {
#ifdef RECORD
        th->fastenq++;
#endif
        return 1;
    } else {
        *id = i;
        return 0;
    }
}

static void enq_slow(queue_t *q, handle_t *th, void *v, long id) {
    enq_t *enq = &th->Er;
    enq->val = v;
    RELEASE(&enq->id, id);

    node_t *tail = th->Ep;
    long i;
    cell_t *c;

    do {
        i = FAA(&q->Ei, 1);
        c = find_cell(&tail, i, th);
        enq_t *ce = BOT;

        if (CAScs(&c->enq, &ce, enq) && c->val != TOP) {
            if (CAS(&enq->id, &id, -i)) id = -i;
            break;
        }
    } while (enq->id > 0);

    id = -enq->id;
    c = find_cell(&th->Ep, id, th);
    if (id > i) {
        long Ei = q->Ei;
        while (Ei <= id && !CAS(&q->Ei, &Ei, id + 1))
            ;
    }
    c->val = v;

#ifdef RECORD
    th->slowenq++;
#endif
}

void enqueue(queue_t *q, handle_t *th, void *v) {
    th->hzd_node_id = th->enq_node_id;

    long id;
    int p = MAX_PATIENCE;
    while (!enq_fast(q, th, v, &id) && p-- > 0)
        ;
    if (p < 0) enq_slow(q, th, v, id);

    th->enq_node_id = th->Ep->id;
    RELEASE(&th->hzd_node_id, -1);
}

static void *help_enq(queue_t *q, handle_t *th, cell_t *c, long i) {
    void *v = spin(&c->val);

    if ((v != TOP && v != BOT) ||
        (v == BOT && !CAScs(&c->val, &v, TOP) && v != TOP)) {
        return v;
    }

    enq_t *e = c->enq;

    if (e == BOT) {
        handle_t *ph;
        enq_t *pe;
        long id;
        ph = th->Eh, pe = &ph->Er, id = pe->id;

        if (th->Ei != 0 && th->Ei != id) {
            th->Ei = 0;
            th->Eh = ph->next;
            ph = th->Eh, pe = &ph->Er, id = pe->id;
        }

        if (id > 0 && id <= i && !CAS(&c->enq, &e, pe))
            th->Ei = id;
        else
            th->Eh = ph->next;

        if (e == BOT && CAS(&c->enq, &e, TOP)) e = TOP;
    }

    if (e == TOP) return (q->Ei <= i ? BOT : TOP);

    long ei = ACQUIRE(&e->id);
    void *ev = ACQUIRE(&e->val);

    if (ei > i) {
        if (c->val == TOP && q->Ei <= i) return BOT;
    } else {
        if ((ei > 0 && CAS(&e->id, &ei, -i)) || (ei == -i && c->val == TOP)) {
            long Ei = q->Ei;
            while (Ei <= i && !CAS(&q->Ei, &Ei, i + 1))
                ;
            c->val = ev;
        }
    }

    return c->val;
}

static void help_deq(queue_t *q, handle_t *th, handle_t *ph) {
    deq_t *deq = &ph->Dr;
    long idx = ACQUIRE(&deq->idx);
    long id = deq->id;

    if (idx < id) return;

    node_t *Dp = ph->Dp;
    th->hzd_node_id = ph->hzd_node_id;
    FENCE();
    idx = deq->idx;

    long i = id + 1, old = id, new = 0;
    while (1) {
        node_t *h = Dp;
        for (; idx == old && new == 0; ++i) {
            cell_t *c = find_cell(&h, i, th);

            long Di = q->Di;
            while (Di <= i && !CAS(&q->Di, &Di, i + 1))
                ;

            void *v = help_enq(q, th, c, i);
            if (v == BOT || (v != TOP && c->deq == BOT))
                new = i;
            else
                idx = ACQUIRE(&deq->idx);
        }

        if (new != 0) {
            if (CASra(&deq->idx, &idx, new)) idx = new;
            if (idx >= new) new = 0;
        }

        if (idx < 0 || deq->id != id) break;

        cell_t *c = find_cell(&Dp, idx, th);
        deq_t *cd = BOT;
        if (c->val == TOP || CAS(&c->deq, &cd, deq) || cd == deq) {
            CAS(&deq->idx, &idx, -idx);
            break;
        }

        old = idx;
        if (idx >= i) i = idx + 1;
    }
}

static void *deq_fast(queue_t *q, handle_t *th, long *id) {
    long i = FAAcs(&q->Di, 1);
    cell_t *c = find_cell(&th->Dp, i, th);
    void *v = help_enq(q, th, c, i);
    deq_t *cd = BOT;

    if (v == BOT) return BOT;
    if (v != TOP && CAS(&c->deq, &cd, TOP)) return v;

    *id = i;
    return TOP;
}

static void *deq_slow(queue_t *q, handle_t *th, long id) {
    deq_t *deq = &th->Dr;
    RELEASE(&deq->id, id);
    RELEASE(&deq->idx, id);

    help_deq(q, th, th);
    long i = -deq->idx;
    cell_t *c = find_cell(&th->Dp, i, th);
    void *val = c->val;

#ifdef RECORD
    th->slowdeq++;
#endif
    return val == TOP ? BOT : val;
}

void *dequeue(queue_t *q, handle_t *th) {
    th->hzd_node_id = th->deq_node_id;

    void *v;
    long id = 0;
    int p = MAX_PATIENCE;

    do
        v = deq_fast(q, th, &id);
    while (v == TOP && p-- > 0);
    if (v == TOP)
        v = deq_slow(q, th, id);
    else {
#ifdef RECORD
        th->fastdeq++;
#endif
    }

    if (v != EMPTY) {
        help_deq(q, th, th->Dh);
        th->Dh = th->Dh->next;
    }

    th->deq_node_id = th->Dp->id;
    RELEASE(&th->hzd_node_id, -1);

    if (th->spare == NULL) {
        cleanup(q, th);
        th->spare = new_node();
    }

#ifdef RECORD
    if (v == EMPTY) th->empty++;
#endif
    return v;
}

void queue_register(queue_t*, handle_t*);

void queue_init(queue_t *q, int nprocs) {
    q->Hi = 0;
    q->Hp = new_node();

    q->Ei = 1;
    q->Di = 1;

    q->nprocs = nprocs;
    q->handles = (handle_t*) malloc(nprocs * sizeof(handle_t));
    for (int i = 0; i < nprocs; i++)
        queue_register(q, &q->handles[i]);

#ifdef RECORD
    q->fastenq = 0;
    q->slowenq = 0;
    q->fastdeq = 0;
    q->slowdeq = 0;
    q->empty = 0;
#endif
}

void queue_free(queue_t *q, handle_t *h) {
#ifdef RECORD
    static int lock = 0;

    FAA(&q->fastenq, h->fastenq);
    FAA(&q->slowenq, h->slowenq);
    FAA(&q->fastdeq, h->fastdeq);
    FAA(&q->slowdeq, h->slowdeq);
    FAA(&q->empty, h->empty);

    if (FAA(&lock, 1) == 0)
        printf("Enq: %f Deq: %f Empty: %f\n",
               q->slowenq * 100.0 / (q->fastenq + q->slowenq),
               q->slowdeq * 100.0 / (q->fastdeq + q->slowdeq),
               q->empty * 100.0 / (q->fastdeq + q->slowdeq));
#endif
}

void queue_register(queue_t *q, handle_t *th) {
    th->next = NULL;
    th->hzd_node_id = -1;
    th->Ep = q->Hp;
    th->enq_node_id = th->Ep->id;
    th->Dp = q->Hp;
    th->deq_node_id = th->Dp->id;

    th->Er.id = 0;
    th->Er.val = BOT;
    th->Dr.id = 0;
    th->Dr.idx = -1;

    th->Ei = 0;
    th->spare = new_node();
#ifdef RECORD
    th->slowenq = 0;
    th->slowdeq = 0;
    th->fastenq = 0;
    th->fastdeq = 0;
    th->empty = 0;
#endif

    static handle_t *volatile _tail;
    handle_t *tail = _tail;

    if (tail == NULL) {
        th->next = th;
        if (CASra(&_tail, &tail, th)) {
            th->Eh = th->next;
            th->Dh = th->next;
            return;
        }
    }

    handle_t *next = tail->next;
    do
        th->next = next;
    while (!CASra(&tail->next, &next, th));

    th->Eh = th->next;
    th->Dh = th->next;
}

/****************************************
 *              izem wrappers
 ****************************************/

extern zm_thread_local int tid;

/* Check the actual affinity mask assigned to the thread */
static inline void check_affinity(hwloc_topology_t topo) {
    hwloc_cpuset_t cpuset = hwloc_bitmap_alloc();
    int set_length;
    hwloc_get_cpubind(topo, cpuset, HWLOC_CPUBIND_THREAD);
    set_length = hwloc_get_nbobjs_inside_cpuset_by_type(topo, cpuset, HWLOC_OBJ_PU);
    hwloc_bitmap_free(cpuset);

    if(set_length != 1) {
        printf("IZEM:WFQUEUE:ERROR: thread bound to more than one HW thread!\n");
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

int zm_wfqueue_init(zm_wfqueue_t *Q) {
    queue_t *q;
    int nprocs;

    posix_memalign((void **) &q, 2 * CACHE_LINE_SIZE, sizeof(queue_t));

    hwloc_topology_init(&q->topo);
    hwloc_topology_load(q->topo);
    nprocs = hwloc_get_nbobjs_by_type(q->topo, HWLOC_OBJ_PU);
    queue_init(q, nprocs);

    *Q = (zm_wfqueue_t) q;
    return 0;
}

int zm_wfqueue_enqueue(zm_wfqueue_t *Q, void *data) {
    queue_t *q = (queue_t*)(void*)(*Q);
    if (zm_unlikely(tid == -1)) {
        check_affinity(q->topo);
        tid = get_hwthread_id(q->topo);
    }
    enqueue(q, &q->handles[tid], data);
    return 0;
}

int zm_wfqueue_dequeue(zm_wfqueue_t *Q, void **data) {
    queue_t *q = (queue_t*)(void*)(*Q);
    if (zm_unlikely(tid == -1)) {
        check_affinity(q->topo);
        tid = get_hwthread_id(q->topo);
    }
    *data = dequeue(q, &q->handles[tid]);
    return 0;
}
