/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "lock/zm_idsmsync.h"

#define TEST_NTEST ((int64_t)10)
#define TEST_NTHREADS ((int64_t)8)
#define TEST_NITER ((int64_t)100000)
#define TEST_LOCAL_OFFSET ((int64_t)24)

int64_t global_val;
int64_t local_vals[TEST_NTHREADS * TEST_LOCAL_OFFSET];

static inline void global_work(int64_t val) {
    global_val += val;
}

static inline void local_work(int64_t thread_id, int64_t val) {
    local_vals[thread_id * TEST_LOCAL_OFFSET] += val;
}

static void work(void *req) {
    int64_t req_val = (intptr_t)(req);
    int64_t thread_id = req_val / TEST_NITER;
    int64_t val = req_val % TEST_NITER;
    global_work(val);
    local_work(thread_id, val);
}

typedef struct {
    pthread_t thread;
    int64_t thread_id;
    int64_t op_kind;
    zm_dsm_t dsm;
} thread_arg_t;

static void *run(void *_arg) {
    thread_arg_t *arg = (thread_arg_t *)_arg;

    const int64_t thread_id = arg->thread_id;
    const int64_t op_kind = arg->op_kind;
    zm_dsm_t dsm = arg->dsm;

    /* Directly change the thread-local value in izem. Not good. */
    tid = thread_id;

    int64_t count;
    for (count = 0; count < TEST_NITER; count++) {
        void *req = (void *)((intptr_t)(arg->thread_id * TEST_NITER + count));
        switch (op_kind) {
        case 0:
            zm_idsm_acquire(dsm);
            work(req);
            zm_idsm_release(dsm);
            break;
        case 1:
            zm_idsm_cacq(dsm, work);
            work(req);
            zm_idsm_release(dsm);
            break;
        case 2:
            while (1) {
                int success = 0;
                zm_idsm_ctry(dsm, work, &success);
                if (success == 1) {
                    work(req);
                    zm_idsm_release(dsm);
                    break;
                }
            }
            break;
        case 3:
        default:
            zm_idsm_sync(dsm, work, req);
            break;
        }
    }
    return NULL;
}

/*-------------------------------------------------------------------------
 * Function: test_idsm_lock
 *
 * Purpose: Test the correctness of idsm lock
 *
 * Return: Success: 0
 *         Failure: 1
 *-------------------------------------------------------------------------
 */
static void test_idsm_lock() {
    thread_arg_t args[TEST_NTHREADS];

    int i;
    for (i = 0; i < TEST_NTEST; i++) {
        zm_dsm_t dsm;
        zm_idsm_init(&dsm);

        global_val = 0;
        memset(local_vals, 0, sizeof(int64_t) * TEST_NTHREADS
                              * TEST_LOCAL_OFFSET);

        int th;
        for (th = 0; th < TEST_NTHREADS; th++) {
            pthread_attr_t attr;
            pthread_attr_init(&attr);
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(th, &cpuset);
            pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset);
            args[th].thread_id = th;
            args[th].op_kind = th % 4;
            args[th].dsm = dsm;
            pthread_create(&args[th].thread, &attr, run, &args[th]);
        }
        for (th = 0; th < TEST_NTHREADS; th++) {
            pthread_join(args[th].thread, NULL);
        }
        zm_idsm_destroy(&dsm);

        /* Examine the result. */
        const int64_t tmp = TEST_NITER * (TEST_NITER - 1) / 2;
        const int64_t global_val_ans = tmp * TEST_NTHREADS;
        if (global_val != global_val_ans) {
            printf("[%d] Error: global_val = %llu (ans %llu)\n", i,
                   (unsigned long long) global_val,
                   (unsigned long long) global_val_ans);
            return;
        }
        for (th = 0; th < TEST_NTHREADS; th++) {
            const int64_t local_val_ans = tmp;
            if (local_vals[th * TEST_LOCAL_OFFSET] != local_val_ans) {
                printf("[%d] Error: local_vals[%d] = %llu (ans %llu)\n", i, th,
                       (unsigned long long) local_vals[th * TEST_LOCAL_OFFSET],
                       (unsigned long long) local_val_ans);
                return;
            }
        }
    }
    printf("Pass\n");
} /* end test_lock_thruput() */

int main(int argc, char **argv)
{
  test_idsm_lock();
} /* end main() */

