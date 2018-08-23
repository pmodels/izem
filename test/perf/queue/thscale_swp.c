/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */
#include <stdlib.h>
#include <stdio.h>
#include <omp.h>
#include "queue/zm_swpqueue.h"
#include "mem/zm_pool.h"
#define TEST_NELEMTS 440
#define NITER (1024*32)

zm_pool_t pool;

/*-------------------------------------------------------------------------
 * Function: run
 *
 * Purpose: Test the correctness of queue operations by counting the number
 *  of dequeued elements to the expected number
 *
 * Return: Success: 0
 *         Failure: 1
 *-------------------------------------------------------------------------
 */
static inline void run() {
    unsigned test_counter = 0;
    zm_swpqueue_t queue;
    double t1, t2;

    printf("#threads \t throughput ops/s\n");

    int nthreads;
    for (nthreads = 2; nthreads <= omp_get_max_threads(); nthreads *= 2) {
        zm_swpqueue_init_explicit(&queue, omp_get_thread_num());
        int nelem_enq, nelem_deq;

        nelem_enq = TEST_NELEMTS/(nthreads-1);
        nelem_deq = (nthreads-1)*nelem_enq;

        t1 = omp_get_wtime();

        #pragma omp parallel num_threads(nthreads)
        {
            int tid, producer_b;
            int *input;
            tid = omp_get_thread_num();
            producer_b = (tid != 0);
            int elem;

            for(int i = 0; i<NITER; i++) {
                if(producer_b) { /* producer */
                    for(elem=0; elem < nelem_enq; elem++) {
                        zm_pool_alloc(pool, tid, (void**)&input);
                        *input = 1;
                        zm_swpqueue_enqueue_explicit(&queue, (void*) input, tid);
                    }
                } else {           /* consumer */
                    while(test_counter < nelem_deq) {
                        int* elem = NULL;
                        zm_swpqueue_dequeue_explicit(&queue, (void**)&elem, tid);
                        if ((elem != NULL) && (*elem == 1)) {
                            #pragma omp atomic
                                test_counter++;
                            zm_pool_free(pool, tid, elem);
                        }
                    }
                }
            }
        }

        t2 = omp_get_wtime();
        printf("%d \t %lf\n", nthreads, (double)nelem_deq*NITER/(t2-t1));
    }

} /* end run() */

int main(int argc, char **argv) {
  zm_pool_create(sizeof(int), &pool);
  run();
} /* end main() */

