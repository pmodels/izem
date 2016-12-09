/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */
#include <stdlib.h>
#include <stdio.h>
#include <omp.h>
#include "lock/zm_lock.h"

#define TEST_NITER (1<<22)
#define WARMUP_ITER 128

#define CACHELINE_SZ 64
#define ARRAY_LEN 4

char cache_lines[CACHELINE_SZ*ARRAY_LEN] = {0};

#if ARRAY_LEN == 10
int indices [] = {3,6,1,7,0,2,9,4,8,5};
#elif ARRAY_LEN == 4
int indices [] = {2,1,3,0};
#endif

static void test_thruput()
{
    unsigned nthreads = omp_get_max_threads();

    zm_lock_t lock;
    zm_lock_init(&lock);
    int cur_nthreads;
    /* Throughput = lock acquisitions per second */
    printf("nthreads,thruput\n");
    for(cur_nthreads=2; cur_nthreads <= nthreads; cur_nthreads+=2) {
        double start_time, stop_time;
        #pragma omp parallel num_threads(cur_nthreads)
        {
            int tid = omp_get_thread_num();
    
            //printf("processing [%d,%d[\n", chunk_start, chunk_end);
    
            zm_lock_ctxt_t ctxt;
            /* Warmup */
            for(int iter=0; iter < WARMUP_ITER; iter++) {
                zm_lock_acquire(&lock, &ctxt);
                /* Computation */
                for(int i = 0; i < ARRAY_LEN; i++)
                     cache_lines[indices[i]] += cache_lines[indices[ARRAY_LEN-1-i]];
                zm_lock_release(&lock, &ctxt);
            }
            #pragma omp barrier
            #pragma omp single
            {
                start_time = omp_get_wtime();
            }
            #pragma omp for schedule(static)
            for(int iter = 0; iter < TEST_NITER; iter++) {
                zm_lock_acquire(&lock, &ctxt);
                /* Computation */
                for(int i = 0; i < ARRAY_LEN; i++)
                     cache_lines[indices[i]] += cache_lines[indices[ARRAY_LEN-1-i]];
                zm_lock_release(&lock, &ctxt);
            }
        }
        stop_time = omp_get_wtime();
        double elapsed_time = stop_time - start_time;
        double thruput = (double)TEST_NITER/elapsed_time;
        printf("%d,%.2lf\n", cur_nthreads, thruput);
    }

}

int main(int argc, char **argv)
{
  test_thruput();
  return 0;
}

