/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "queue/zm_faqueue.h"

/*
     Helper functions
 */

static inline zm_faseg_t *zm_faseg_alloc(zm_ulong_t seg_id) {
    zm_faseg_t *seg = malloc (sizeof(zm_faseg_t));
    seg->id = seg_id;
    atomic_store_explicit(&seg->next, ZM_NULL, memory_order_release);
    for(int i = 0; i < ZM_MAX_FASEG_SIZE; i++)
        seg->cells[i].data = ZM_FAQUEUE_ALPHA;
    return seg;
}

static inline zm_faseg_t *zm_faseg_advance(zm_faseg_t *seg, zm_ulong_t seg_id){
    zm_faseg_t *next =  (zm_faseg_t*) atomic_load_explicit(&seg->next,
                                                           memory_order_acquire);
    if((zm_ptr_t)next == ZM_NULL) {
        zm_faseg_t *tmp_seg = zm_faseg_alloc (seg_id);
        if (!atomic_compare_exchange_weak_explicit(&seg->next,
                                                  (zm_ptr_t*)&next,
                                                  (zm_ptr_t)tmp_seg,
                                                  memory_order_release,
                                                  memory_order_acquire))
            free(tmp_seg);
    
        next = (zm_faseg_t*) atomic_load_explicit(&seg->next,
                                                  memory_order_acquire);
    }
    return next;
}

/* Search, and allocate a segment if necessary, the cell that cell_id belongs to*/
static inline zm_facell_t *zm_facell_find(zm_faseg_t **seg, zm_ulong_t cell_id) {
    zm_faseg_t *cur_seg = *seg;
    zm_ulong_t trg_seg = cell_id/ZM_MAX_FASEG_SIZE;

    for (zm_ulong_t i = cur_seg->id; i < trg_seg; i++)
        cur_seg = zm_faseg_advance (cur_seg, i + 1);

    *seg = cur_seg;
    return &cur_seg->cells[cell_id % ZM_MAX_FASEG_SIZE];
}

/*
   Body of the routines
 */

int zm_faqueue_init(zm_faqueue_t *q) {
    q->head = 0;
    atomic_store(&q->tail, 0);
    q->seg_head = (zm_ptr_t)zm_faseg_alloc(0);
    atomic_store(&q->seg_tail, q->seg_head);
    return 0;
}

int zm_faqueue_enqueue(zm_faqueue_t* q, void *data) {
    zm_ulong_t cell_id = atomic_fetch_add_explicit(&q->tail, 1, memory_order_acq_rel);
    zm_facell_t *cell = zm_facell_find ((zm_faseg_t**)&q->seg_tail, cell_id);
    cell->data = data;
    return 0;
}

int zm_faqueue_dequeue(zm_faqueue_t* q, void **data) {
    *data = NULL;
    zm_faseg_t *seg_head = (zm_faseg_t*)q->seg_head;
    if (seg_head->cells[q->head].data == ZM_FAQUEUE_ALPHA) /* queue empty */
        return 0;
    else {
        *data = seg_head->cells[q->head].data;
        q->head++;
        if (q->head % ZM_MAX_FASEG_SIZE == 0) { /* processed all seg_head */
            q->seg_head = (zm_ptr_t)zm_faseg_advance(seg_head, q->head);
            free(seg_head);
        }
    }
    return 1;
}