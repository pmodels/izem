/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */
#include <assert.h>
#include "queue/zm_mpbqueue.h"
#include "queue/zm_swpqueue.h"

#define EMPTY_BUCKET 0
#define NONEMPTY_BUCKET 1
#define INCONSISTENT_BUCKET 2

#define LOAD(addr)                  zm_atomic_load(addr, zm_memord_acquire)
#define STORE(addr, val)            zm_atomic_store(addr, val, zm_memord_release)
#define SWAP(addr, desire)          zm_atomic_exchange(addr, desire, zm_memord_acq_rel)
#define CAS(addr, expect, desire)   zm_atomic_compare_exchange_strong(addr,\
                                                                      expect,\
                                                                      desire,\
                                                                      zm_memord_acq_rel,\
                                                                      zm_memord_acquire)


int zm_mpbqueue_init(struct zm_mpbqueue *q, int nbuckets) {
    /* multiples of 8 allow single operation, 64bit-width emptiness check  */
    /* TODO: replace the below assert with error handling */
    int llong_width = (int) sizeof(zm_atomic_llong_t);
    assert(nbuckets % llong_width == 0);

    /* allocate and initialize the buckets as SWP-based MPSC queues */
    zm_swpqueue_t *buckets = (zm_swpqueue_t*) malloc(sizeof(zm_swpqueue_t) * nbuckets);
    for(int i = 0; i < nbuckets; i++)
        zm_swpqueue_init(&buckets[i]);
    /* initialze the state of all buckests to empty (0) */
    zm_atomic_llong_t *bucket_state_sets = (zm_atomic_llong_t*) malloc(sizeof(zm_atomic_char_t) * nbuckets);
    for(int i = 0; i < nbuckets/llong_width; i++)
        STORE(&bucket_state_sets[i], EMPTY_BUCKET);

    q->buckets = buckets;
    q->nbuckets = nbuckets;
    q->bucket_states = (zm_atomic_char_t*) bucket_state_sets;

    return 0;
}

int zm_mpbqueue_enqueue(struct zm_mpbqueue* q, void *data, int bucket_idx) {

//    if(zm_swpqueue_isempty(&q->buckets[bucket_idx]))
//        STORE(&q->bucket_states[bucket_idx], INCONSISTENT_BUCKET);

    /* Push to the queue at bucket_idx*/
    zm_swpqueue_enqueue(&q->buckets[bucket_idx], data);
   // printf("enq\n");//fflush(stdout);

    /* check first that the queue is not empty before setting the non-empty flag
     * the consumer might have already dequeued the last element that was just enqeueud above,
     * in which case, setting the non-empty flag is unnecessary. */
    if(!zm_swpqueue_isempty_weak(&q->buckets[bucket_idx])) {
        STORE(&q->bucket_states[bucket_idx], NONEMPTY_BUCKET);
  //      printf("signal\n");//fflush(stdout);
    }

    return 0;
}

int zm_mpbqueue_dequeue(struct zm_mpbqueue* q, void **data) {

    *data = NULL;

    /* Check for a nonempty bucket in sets of bucket_setsz */
    int llong_width  = (int) sizeof(zm_atomic_llong_t);
    int nbucket_sets = q->nbuckets/llong_width;
    int bucket_setsz = llong_width/sizeof(zm_atomic_char_t);

    zm_atomic_llong_t *bucket_state_sets = (zm_atomic_llong_t *)q->bucket_states;
    int i;
    for(i = 0; i < nbucket_sets; i++) {
        int offset = (q->last_bucket_set + i) % nbucket_sets;
        if(LOAD(&bucket_state_sets[offset]) > EMPTY_BUCKET) {
            int j;
            for(j = 0; j < bucket_setsz; j++) {
                int bucket_idx = offset * bucket_setsz + j;
                if (LOAD(&q->bucket_states[bucket_idx]) == NONEMPTY_BUCKET) {
       //            printf("recv\n");//fflush(stdout);
       //            assert(!zm_swpqueue_isempty(&q->buckets[bucket_idx]));
                    zm_swpqueue_dequeue(&q->buckets[bucket_idx], data);
       //             assert(*data != NULL);
                    /* soft check on emptiness */
                    if(zm_swpqueue_isempty_weak(&q->buckets[bucket_idx])) {
                        /* heavier check to confirm emptiness */
                        if(zm_swpqueue_isempty_strong(&q->buckets[bucket_idx])) {
                            /* reset the bucket state to avoid checking it again if it stays empty */
                            STORE(&q->bucket_states[bucket_idx], EMPTY_BUCKET);
       //                     printf("reset\n");//fflush(stdout);
                        }
                    }
                    break;
                }
            }
            if(j < bucket_setsz)
                break;
        }
    }
    q->last_bucket_set = (q->last_bucket_set + i) % nbucket_sets;
    return 1;
}

int zm_mpbqueue_dequeue_bulk(struct zm_mpbqueue* q, void **data, int in_count, int *out_count) {
    /* Check for a nonempty bucket in sets of bucket_setsz */
    int llong_width  = (int) sizeof(zm_atomic_llong_t);
    int nbucket_sets = q->nbuckets/llong_width;
    int bucket_setsz = llong_width/sizeof(zm_atomic_char_t);

    zm_atomic_llong_t *bucket_state_sets = (zm_atomic_llong_t *)q->bucket_states;
    int i, out_idx = 0;
    for(i = 0; i < nbucket_sets; i++) {
        int offset = (q->last_bucket_set + i) % nbucket_sets;
        if(LOAD(&bucket_state_sets[offset]) > EMPTY_BUCKET) {
            int j;
            for(j = 0; j < bucket_setsz; j++) {
                int bucket_idx = offset * bucket_setsz + j;
                if (LOAD(&q->bucket_states[bucket_idx]) == NONEMPTY_BUCKET) {
                    zm_swpqueue_dequeue(&q->buckets[bucket_idx], &data[out_idx]);
                    out_idx++;
                    if(zm_swpqueue_isempty_strong(&q->buckets[bucket_idx])) {
                        char tmp = NONEMPTY_BUCKET;
                        CAS(&q->bucket_states[bucket_idx], &tmp, EMPTY_BUCKET);
                    }
                    if(out_idx >= in_count)
                        break;
                }
            }
            if(j < bucket_setsz)
                break;
        }
    }
    q->last_bucket_set = (q->last_bucket_set + i) % nbucket_sets;
    *out_count = out_idx + 1;
    return 1;
}
