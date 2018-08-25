/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <mem/zm_pool.h>

#define TEST_NTHREADS        8
#define TEST_NTHREADS_PARENT 8
#define TEST_NELEMTS   20000
#define NUM_OPERATIONS 30000

typedef struct thread_data thread_data_t;
struct thread_data {
    int tid;
    size_t element_size;
    zm_pool_t pool;
};

static void* func(void *arg) {
    thread_data_t *data = (thread_data_t*) arg;
    const int tid = data->tid;
    const int id = tid % 128;
    const size_t element_size = data->element_size;
    zm_pool_t pool = data->pool;

    unsigned short xsubi[3] = {tid, id, element_size};
    void **ptrs = (void *)malloc(sizeof(void *) * TEST_NELEMTS);

    int r = 0, num_elements = 0;
    do {
        double rand = erand48(xsubi);
        if (rand < 0.5) {
            // Allocate elements
            int num_allocs = (rand * 2) * 1000;
            if (num_elements + num_allocs >= TEST_NELEMTS) {
                num_allocs = TEST_NELEMTS - num_elements;
            }
            if (num_allocs == 0)
                continue;
            for (int i = 0; i < num_allocs; i++) {
                void *ptr = NULL;
                zm_pool_alloc(pool, &ptr);
                __builtin_memset(ptr, id, element_size);
                ptrs[num_elements++] = ptr;
            }
        } else {
            // Free elements
            int num_frees = ((rand - 0.5)* 2) * 1000;
            if (num_frees > num_elements) {
                num_frees = num_elements;
            }
            if (num_frees == 0)
                continue;
            for (int i = 0; i < num_frees; i++) {
                void *ptr = ptrs[--num_elements];
                for (int j = 0; j < (int)element_size; j++) {
                    char val = ((char *)ptr)[j];
                    if (val != id) {
                        printf("Failed: got ptrs[%d][%d] %d instead of %d\n", num_elements, j, (int) val, id);
                        abort();
                    }
                }
                zm_pool_free(pool, ptr);
            }
        }

    } while(r++ <= NUM_OPERATIONS);

    // Free remaining elements.
    for (int i = 0; i < num_elements; i++) {
        void *ptr = ptrs[i];
        for (int j = 0; j < (int)element_size; j++) {
            char val = ((char *)ptr)[j];
            if (val != id) {
                printf("Failed: got ptrs[%d][%d] %d instead of %d\n", i, j, (int) val, id);
                abort();
            }
        }
        zm_pool_free(pool, ptr);
    }

    free(ptrs);
    return 0;
}

/*-------------------------------------------------------------------------
 * Function: run
 *
 * Purpose: Test the pool operations by repeating alloc() and free() by
 *          multiple threads.
 *
 * Return: Success: 0
 *         Failure: 1
 *-------------------------------------------------------------------------
 */
static void *run(void *arg) {
    pthread_t threads[TEST_NTHREADS];
    thread_data_t data[TEST_NTHREADS];

    // const size_t element_sizes[] = {1, 64, 256, 1024};
    const size_t element_sizes[] = {1, 2, 4, 8, 16 ,7 ,5, 3, 2, 1, 64, 128, 256, 1024, 1, 3, 5, 6, 8};
    // const size_t element_sizes[] = {1, 2};

    for (int i = 0; i < sizeof(element_sizes) / sizeof(const size_t); i++) {
        zm_pool_t pool;
        zm_pool_create(element_sizes[i], &pool);
        printf("trying %d\n", (int) element_sizes[i]);
        for (int tid = 0; tid < TEST_NTHREADS; tid++) {
            data[tid].tid = tid;
            data[tid].element_size = element_sizes[i];
            data[tid].pool = pool;
        }
        for (int tid = 1; tid < TEST_NTHREADS; tid++)
            pthread_create(&threads[tid], NULL, func, (void*) &data[tid]);
        func(&data[0]);
        for (int tid = 1; tid < TEST_NTHREADS; tid++)
            pthread_join(threads[tid], NULL);

        zm_pool_destroy(&pool);
    }
    return NULL;
}

int main(int argc, char **argv) {
    pthread_t threads[TEST_NTHREADS_PARENT];
    for (int tid = 1; tid < TEST_NTHREADS_PARENT; tid++)
        pthread_create(&threads[tid], NULL, run, NULL);
    run(NULL);
    for (int tid = 1; tid < TEST_NTHREADS_PARENT; tid++)
        pthread_join(threads[tid], NULL);
}
