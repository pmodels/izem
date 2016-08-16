/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef _ZM_HZDPTR_H_
#define _ZM_HZDPTR_H_
#include "common/zm_common.h"
#include "list/zm_sdlist.h"

/* several lock-free objects only require one or two hazard pointers,
 * thus the current default value. We might increase it in the future
 * in case it is no longer sufficient. */
#define ZM_HZDPTR_NUM 2 /* K */

typedef zm_ptr_t zm_hzdptr_t;
/* Forward declaration(s) */
typedef struct zm_hzdptr_lnode zm_hzdptr_lnode_t;

/* List to store hazard pointers. This list simulates a dynamic array.
 * It is lock-free and only supports the push_back() operation */
struct zm_hzdptr_lnode {
    zm_hzdptr_t hzdptrs[ZM_HZDPTR_NUM];
    atomic_flag active;
    zm_sdlist_t rlist;
    zm_atomic_ptr_t next;
};

extern zm_atomic_ptr_t zm_hzdptr_list; /* head of the list*/
extern atomic_uint zm_hplist_length; /* N: correlates with the number
                                        of threads */

/* per-thread pointer to its own hazard pointer node */
extern zm_thread_local zm_hzdptr_lnode_t* zm_my_hplnode;

static inline void zm_hzdptr_allocate() {
    zm_hzdptr_lnode_t *cur_hplnode = zm_hzdptr_list;
    zm_ptr_t old_hplhead;
    while (cur_hplnode != NULL) {
        if(atomic_flag_test_and_set_explicit(&cur_hplnode->active, memory_order_acq_rel)) {
            cur_hplnode = (zm_hzdptr_lnode_t*)atomic_load_explicit(&cur_hplnode->next, memory_order_acquire);
            continue;
        }
        zm_my_hplnode = cur_hplnode;
        return;
    }
    atomic_fetch_add_explicit(&zm_hplist_length, 1, memory_order_acq_rel);
    /* Allocate and initialize a new node */
    cur_hplnode = malloc (sizeof *cur_hplnode);
    atomic_flag_test_and_set_explicit(&cur_hplnode->active, memory_order_acq_rel);
    atomic_store_explicit(&cur_hplnode->next, NULL, memory_order_release);
    do {
        old_hplhead = atomic_load_explicit(&zm_hzdptr_list, memory_order_acquire);
        atomic_store_explicit(&cur_hplnode->next, old_hplhead, memory_order_release);
    } while(!atomic_compare_exchange_weak_explicit(&zm_hzdptr_list,
                                                   &old_hplhead,
                                                   (zm_ptr_t)cur_hplnode,
                                                   memory_order_release,
                                                   memory_order_acquire));
    zm_my_hplnode = cur_hplnode;
    for(int i=0; i<ZM_HZDPTR_NUM; i++)
        zm_my_hplnode->hzdptrs[i] = NULL;
    zm_sdlist_init(&zm_my_hplnode->rlist);
}

static inline void zm_hzdptr_scan() {
    zm_sdlist_t plist;
    /* stage 1: pull non-null values from hzdptr_list */
    zm_sdlist_init(&plist);
    zm_hzdptr_lnode_t *cur_hplnode = zm_hzdptr_list;
    while(cur_hplnode != NULL) {
        for(int i=0; i<ZM_HZDPTR_NUM; i++) {
            if(cur_hplnode->hzdptrs[i] != NULL)
                zm_sdlist_push_back(&plist, cur_hplnode->hzdptrs[i]);
        }
        cur_hplnode = (zm_hzdptr_lnode_t*)atomic_load_explicit(&cur_hplnode->next, memory_order_acquire);
    }
    /* stage 2: search plist */
    zm_sdlnode_t *node = zm_sdlist_begin(zm_my_hplnode->rlist);
    while (node != NULL) {
        zm_sdlnode_t *next = zm_sdlist_next(*node);
        if(!zm_sdlist_remove(&plist, ((zm_sdlnode_t*)node)->data))
            /* TODO: reuse instead of freeing */
            zm_sdlist_rmnode(&zm_my_hplnode->rlist, (zm_sdlnode_t*)node);
        node = next;
    }
    zm_sdlist_free(&plist);
}

static inline void zm_hzdptr_helpscan() {
    zm_hzdptr_lnode_t *cur_hplnode = zm_hzdptr_list;
    while(cur_hplnode != NULL) {
        if(atomic_flag_test_and_set_explicit(&cur_hplnode->active, memory_order_acq_rel)) {
            cur_hplnode = (zm_hzdptr_lnode_t*)atomic_load_explicit(&cur_hplnode->next, memory_order_acquire);
            continue;
        }
        while(zm_sdlist_length(cur_hplnode->rlist) > 0) {
            zm_ptr_t node = zm_sdlist_pop_front(&cur_hplnode->rlist);
            zm_sdlist_push_back(&zm_my_hplnode->rlist, node);
            if(zm_sdlist_length(zm_my_hplnode->rlist) >=
               2 * atomic_load_explicit(&zm_hplist_length, memory_order_acquire))
                zm_hzdptr_scan();
        }
        atomic_flag_clear_explicit(&cur_hplnode->active, memory_order_release);
        cur_hplnode = (zm_hzdptr_lnode_t*)atomic_load_explicit(&cur_hplnode->next, memory_order_acquire);
    }
}

static inline void zm_hzdptr_retire(zm_atomic_ptr_t node) {
    zm_sdlist_push_back(&zm_my_hplnode->rlist, node);
    if(zm_sdlist_length(zm_my_hplnode->rlist) >=
       2 * atomic_load_explicit(&zm_hplist_length, memory_order_acquire)) {
        zm_hzdptr_scan();
        zm_hzdptr_helpscan();
    }
}

static inline zm_hzdptr_t* zm_hzdptr_get() {
    if(zm_unlikely(zm_my_hplnode == NULL))
        zm_hzdptr_allocate();
    return zm_my_hplnode->hzdptrs;
}

#endif /* _ZM_HZDPTR_H_ */
