/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "mem/zm_pool.h"
#include <string.h>
#include <hwloc.h>

// Bulk size (bytes)
#define PAGESIZE (4 * 1024 * 1024)
// Block size
#define BLOCKSIZE 256
// Local pool capacity
#define LOCALPOOL_NUM_BLOCKS 2
// Number of blocks that are taken from a global pool.
#define GLOBAL_TO_LOCAL_NUM_BLOCKS 1
// Number of blocks that are returned to a global pool.
#define LOCAL_TO_GLOBAL_NUM_BLOCKS 1

struct element {
    struct element *next;
    // element is the first sizeof(element) bytes of an element.
};

struct memory_bulk {
    struct memory_bulk *next;
    // memory_bulk is the first sizeof(memory_bulk) bytes of a page.
} __attribute__((aligned(ZM_CACHELINE_SIZE)));

struct block {
    int num_elements;
    struct element *head;
    struct element *tail;
};

struct local_pool {
    int num_elements;
    struct block *blocks;
} __attribute__((aligned(ZM_CACHELINE_SIZE)));

struct global_pool {
    // Assumes tids range from [0.. max_threads)
    size_t element_size;
    int max_threads;
    struct local_pool **local_pools;
    __attribute__((aligned(ZM_CACHELINE_SIZE)))
    int lock;
    __attribute__((aligned(ZM_CACHELINE_SIZE)))
    int num_elements;
    int len_blocks;
    struct block *blocks;
    struct memory_bulk *bulk;
} __attribute__((aligned(ZM_CACHELINE_SIZE)));

static inline void *element_to_ptr(struct element *element) {
    return (void *)(((char *)element) + sizeof(struct element));
}

static inline struct element *ptr_to_element(void *ptr) {
    return (struct element *)(((char *)ptr) - sizeof(struct element));
}

static inline void *allocate(size_t size) {
    void *ptr;
    if (posix_memalign(&ptr, ZM_CACHELINE_SIZE, size))
        return NULL;
    return ptr;
}

static inline int get_max_threads() {
    hwloc_topology_t topo;
    hwloc_topology_init(&topo);
    hwloc_topology_load(topo);
    int max_threads = hwloc_get_nbobjs_by_type(topo, HWLOC_OBJ_PU);
    hwloc_topology_destroy(topo);
    return max_threads;
}

static inline void lock_create(struct global_pool *global_pool) {
    char *p_lock = (char *)&global_pool->lock;
    *p_lock = 0;
}

static inline void lock_acquire(struct global_pool *global_pool) {
    char *p_lock = (char *)&global_pool->lock;
    while (zm_atomic_flag_test_and_set(p_lock, zm_memord_acquire) == 1) {
        while (zm_atomic_load(p_lock, zm_memord_acquire) != 0) {
            ; // pause?
        }
    }
}

static inline void lock_release(struct global_pool *global_pool) {
    char *p_lock = (char *)&global_pool->lock;
    zm_atomic_flag_clear(p_lock, zm_memord_release);
}

static inline void lock_destroy(struct global_pool *global_pool) {
    // Do nothing.
}

static struct local_pool *local_pool_create(size_t element_size) {
    struct local_pool *local_pool = (struct local_pool *)allocate(sizeof(struct local_pool));
    local_pool->num_elements = 0;
    local_pool->blocks = (struct block *)allocate(sizeof(struct block) * LOCALPOOL_NUM_BLOCKS);
    memset(local_pool->blocks, 0, sizeof(struct block) * LOCALPOOL_NUM_BLOCKS);
    return local_pool;
}

static void local_pool_destroy(struct local_pool *local_pool) {
    free(local_pool->blocks);
    free(local_pool);
}

int zm_pool_create(size_t element_size, zm_pool_t *handle) {
    struct global_pool *global_pool = (struct global_pool *)allocate(sizeof(struct global_pool));
    global_pool->element_size = element_size;
    global_pool->max_threads = get_max_threads();
    global_pool->local_pools = (struct local_pool **)allocate(sizeof(struct local_pool *) * global_pool->max_threads);
    for (int tid = 0; tid < global_pool->max_threads; tid++) {
        global_pool->local_pools[tid] = local_pool_create(element_size);
    }
    lock_create(global_pool);
    global_pool->num_elements = 0;
    global_pool->len_blocks = 0;
    global_pool->blocks = NULL;
    global_pool->bulk = NULL;
    *handle = (zm_pool_t)global_pool;
    return 0;
}

int zm_pool_destroy(zm_pool_t *handle) {
    struct global_pool *global_pool = *(struct global_pool **)handle;
    // Free local pools.
    for (int tid = 0; tid < global_pool->max_threads; tid++) {
        local_pool_destroy(global_pool->local_pools[tid]);
    }
    free(global_pool->local_pools);
    // Free lock.
    lock_destroy(global_pool);
    // Free blocks.
    free(global_pool->blocks);
    // Free bulks.
    struct memory_bulk *bulk = global_pool->bulk;
    while (bulk) {
        struct memory_bulk *next = bulk->next;
        free(bulk);
        bulk = next;
    }

    free(global_pool);
    return 0;
}

int zm_pool_alloc(zm_pool_t handle, int tid, void **ptr) {
    struct global_pool *global_pool = (struct global_pool *)handle;
    struct local_pool *local_pool = global_pool->local_pools[tid];
    if (local_pool->num_elements == 0) {
        lock_acquire(global_pool);
        while (global_pool->num_elements < GLOBAL_TO_LOCAL_NUM_BLOCKS * BLOCKSIZE) {
            // Allocate a new bulk.
            const size_t element_size = ((sizeof(struct element) + global_pool->element_size + ZM_CACHELINE_SIZE - 1) / ZM_CACHELINE_SIZE) * ZM_CACHELINE_SIZE;
            size_t bulk_size = PAGESIZE;
            char *mem = (char *)allocate(bulk_size);
            struct memory_bulk *bulk = (struct memory_bulk *)mem;
            mem += sizeof(struct memory_bulk);
            bulk_size -= sizeof(struct memory_bulk);
            // Add a new bulk.
            bulk->next = global_pool->bulk;
            global_pool->bulk = bulk;
            // Create elements.
            int block_i = global_pool->num_elements / BLOCKSIZE;
            struct block *blocks = global_pool->blocks;
            int len_blocks = global_pool->len_blocks;
            int num_elements = global_pool->num_elements;
            while (bulk_size >= element_size) {
                struct element *new_element = (struct element *)mem;
                new_element->next = NULL;
                mem += element_size;
                bulk_size -= element_size;
                if (block_i >= len_blocks) {
                    // Extend blocks.
                    int new_len_blocks = (len_blocks == 0) ? 1 : (len_blocks * 2);
                    struct block *new_blocks = (struct block *)allocate(sizeof(struct block) * new_len_blocks);
                    memcpy(new_blocks, blocks, sizeof(struct block) * len_blocks);
                    memset(&new_blocks[len_blocks], 0, sizeof(struct block) * (new_len_blocks - len_blocks));
                    len_blocks = new_len_blocks;
                    free(blocks);
                    blocks = new_blocks;
                }
                // Put it to a tail block.
                struct block *block = &blocks[block_i];
                if (block->num_elements == 0) {
                    block->head = new_element;
                    block->tail = new_element;
                } else {
                    block->tail->next = new_element;
                    block->tail = new_element;
                }
                block->num_elements++;
                if (block->num_elements == BLOCKSIZE)
                    block_i++;
                num_elements++;
            }
            global_pool->blocks = blocks;
            global_pool->len_blocks = len_blocks;
            global_pool->num_elements = num_elements;
        }
        // Take blocks from a global pool.
        int global_block_i = global_pool->num_elements / BLOCKSIZE;
        memcpy(local_pool->blocks, &global_pool->blocks[global_block_i - GLOBAL_TO_LOCAL_NUM_BLOCKS], sizeof(struct block) * GLOBAL_TO_LOCAL_NUM_BLOCKS);
        local_pool->num_elements = BLOCKSIZE * GLOBAL_TO_LOCAL_NUM_BLOCKS;
        global_pool->num_elements -= BLOCKSIZE * GLOBAL_TO_LOCAL_NUM_BLOCKS;
        if (global_pool->num_elements % BLOCKSIZE != 0) {
            // Copy the last block of global_pool and clear the remaining blocks.
            memcpy(&global_pool->blocks[global_block_i - GLOBAL_TO_LOCAL_NUM_BLOCKS], &global_pool->blocks[global_block_i], sizeof(struct block));
            memset(&global_pool->blocks[global_block_i - GLOBAL_TO_LOCAL_NUM_BLOCKS + 1], 0, sizeof(struct block) * (global_pool->len_blocks - (global_block_i - GLOBAL_TO_LOCAL_NUM_BLOCKS + 1)));
        } else {
            // Clear the remaining blocks.
            memset(&global_pool->blocks[global_block_i - GLOBAL_TO_LOCAL_NUM_BLOCKS], 0, sizeof(struct block) * (global_pool->len_blocks - (global_block_i - GLOBAL_TO_LOCAL_NUM_BLOCKS)));
        }
        lock_release(global_pool);
    }
    {
        // Take an element from a local pool.
        local_pool->num_elements -= 1;
        int block_i = local_pool->num_elements / BLOCKSIZE;
        struct block *block = &local_pool->blocks[block_i];
        struct element *element = block->head;
        struct element *next = element->next;
        block->head = next;
        block->num_elements -= 1;
        *ptr = element_to_ptr(element);
    }
    return 0;
}

int zm_pool_free(zm_pool_t handle, int tid, void *ptr) {
    struct global_pool *global_pool = (struct global_pool *)handle;
    struct local_pool *local_pool = global_pool->local_pools[tid];
    if (local_pool->num_elements == LOCALPOOL_NUM_BLOCKS * BLOCKSIZE) {
        lock_acquire(global_pool);
        int len_required_blocks = (global_pool->num_elements + BLOCKSIZE - 1 + LOCAL_TO_GLOBAL_NUM_BLOCKS * BLOCKSIZE) / BLOCKSIZE;
        struct block* blocks = global_pool->blocks;
        if (global_pool->len_blocks < len_required_blocks) {
            // Extend blocks.
            const int len_blocks = global_pool->len_blocks;
            int new_len_blocks = (len_blocks * 2 < len_required_blocks) ? len_required_blocks : (len_blocks * 2);
            struct block *new_blocks = (struct block *)allocate(sizeof(struct block) * new_len_blocks);
            memcpy(new_blocks, blocks, sizeof(struct block) * len_blocks);
            memset(&new_blocks[len_blocks], 0, sizeof(struct block) * (new_len_blocks - len_blocks));
            global_pool->len_blocks = new_len_blocks;
            free(blocks);
            blocks = new_blocks;
            global_pool->blocks = new_blocks;
        }
        // Return blocks to a global pool.
        int block_i = global_pool->num_elements / BLOCKSIZE;
        if (global_pool->num_elements % BLOCKSIZE != 0) {
            // Copy the last block of a global pool.
            memcpy(&blocks[block_i + LOCAL_TO_GLOBAL_NUM_BLOCKS], &blocks[block_i], sizeof(struct block));
        }
        memcpy(&blocks[block_i], &local_pool->blocks[LOCALPOOL_NUM_BLOCKS - LOCAL_TO_GLOBAL_NUM_BLOCKS], sizeof(struct block) * LOCAL_TO_GLOBAL_NUM_BLOCKS);
        memset(&local_pool->blocks[LOCALPOOL_NUM_BLOCKS - LOCAL_TO_GLOBAL_NUM_BLOCKS], 0, sizeof(struct block) * LOCAL_TO_GLOBAL_NUM_BLOCKS);
        global_pool->num_elements += LOCAL_TO_GLOBAL_NUM_BLOCKS * BLOCKSIZE;
        local_pool->num_elements -= LOCAL_TO_GLOBAL_NUM_BLOCKS * BLOCKSIZE;
        lock_release(global_pool);
    }
    {
        // Return an element to a local pool.
        int block_i = local_pool->num_elements / BLOCKSIZE;
        local_pool->num_elements += 1;
        struct block *block = &local_pool->blocks[block_i];
        struct element *element = ptr_to_element(ptr);
        if (block->num_elements == 0) {
            block->head = element;
            block->tail = element;
            element->next = NULL;
        } else {
            element->next = block->head;
            block->head = element;
        }
        block->num_elements += 1;
    }
    return 0;
}

