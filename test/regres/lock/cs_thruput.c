/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <lock/zm_ticket.h>

#define TEST_NTHREADS 4
#define TEST_NITER 100000

static void* run(void *arg) {
     zm_ticket_t *lock = (zm_ticket_t*) arg;
     for(int iter=0; iter<TEST_NITER; iter++) {
         int err =  zm_ticket_acquire(lock);
         if(err==0)  /* Lock successfully acquired */
            zm_ticket_release(lock);   /* Release the lock */
         else {
            fprintf(stderr, "Error: couldn't acquire the lock\n");
            exit(1);
         }
     }
     return 0;
}

/*-------------------------------------------------------------------------
 * Function: test_lock_throughput
 *
 * Purpose: Test the lock thruput for an empty critical section
 *
 * Return: Success: 0
 *         Failure: 1
 *-------------------------------------------------------------------------
 */
static void test_lock_thruput() {
    void *res;
    pthread_t threads[TEST_NTHREADS];

    zm_ticket_t lock;
    zm_ticket_init(&lock);

    for (int th=0; th<TEST_NTHREADS; th++)
        pthread_create(&threads[th], NULL, run, (void*) &lock);
    for (int th=0; th<TEST_NTHREADS; th++)
        pthread_join(threads[th], &res);

    printf("Pass\n");

} /* end test_lock_thruput() */

int main(int argc, char **argv)
{
  test_lock_thruput();
} /* end main() */

