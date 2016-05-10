/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "zmtest_absqueue.h"
#define TEST_NTHREADS 4
#define TEST_NELEMTS  1000

typedef struct thread_data thread_data_t;
struct thread_data {
    int tid;
    zm_absqueue_t* queue;
};

int input = 1;
atomic_uint test_counter = 0;

static void* func(void *arg) {
    int tid;
    zm_absqueue_t* queue;
    thread_data_t *data = (thread_data_t*) arg;
    tid   = data->tid;
    queue = data->queue;

    if(tid % 2 != 0) { /* producer(s) */
        for(int elem=0; elem < TEST_NELEMTS; elem++) {
            zm_absqueue_enqueue(queue, (void*) &input);
        }
    } else {           /* consumer(s) */
        while(atomic_load_explicit(&test_counter, memory_order_acquire) < (TEST_NTHREADS/2)*TEST_NELEMTS) {
            int* elem = NULL;
            zm_absqueue_dequeue(queue, (void**)&elem);
            if ((elem != NULL) && (*elem == 1))
                    atomic_fetch_add_explicit(&test_counter, 1, memory_order_acq_rel);
        }

    }
    return 0;
}

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
static void run() {
    void *res;
    pthread_t threads[TEST_NTHREADS];
    thread_data_t data[TEST_NTHREADS];

    zm_absqueue_t queue;
    zm_absqueue_init(&queue);

    atomic_store(&test_counter, 0);

    for (int th=0; th < TEST_NTHREADS; th++) {
        data[th].tid = th;
        data[th].queue = &queue;
        pthread_create(&threads[th], NULL, func, (void*) &data[th]);
    }
    for (int th=0; th < TEST_NTHREADS; th++)
        pthread_join(threads[th], &res);

    if(test_counter != (TEST_NTHREADS/2)*TEST_NELEMTS)
        printf("Failed: got counter %d instead of %d\n", test_counter, (TEST_NTHREADS/2)*TEST_NELEMTS);
    else
        printf("Pass\n");

} /* end test_lock_thruput() */

int main(int argc, char **argv)
{
  run();
} /* end main() */

