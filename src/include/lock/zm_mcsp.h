/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */
#include "zm_mcs.h"
#include "zm_ticket.h"

int zm_mcsp_init(zm_mcsp_t *L);
int zm_mcsp_acquire(zm_mcsp_t *L, zm_mcs_qnode_t* I);
int zm_mcsp_acquire_low(zm_mcsp_t *L, zm_mcs_qnode_t* I);
int zm_mcsp_release(zm_mcsp_t *L, zm_mcs_qnode_t *I);
