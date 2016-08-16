/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef _ZM_COMMON_H
#define _ZM_COMMON_H

#if defined(__x86_64__)
#define ZM_CACHELINE_SIZE 64
#else
#error "Check your CPU architecture and set the appropriate macros in util.h"
#endif

#define ZM_ALLIGN_TO_CACHELINE __attribute__((aligned(ZM_CACHELINE_SIZE)))

#define zm_ptr_t void*

#if !defined(__STDC_NO_ATOMICS__)
#include <stdatomic.h>

#define zm_atomic_ptr_t _Atomic(zm_ptr_t)

#else
#include <opa_primitives.h>
#endif

#if (__STDC_VERSION__ >= 201112L)
#define zm_thread_local _Thread_local
#endif

#define zm_likely(x)      __builtin_expect(!!(x), 1)
#define zm_unlikely(x)    __builtin_expect(!!(x), 0)

#endif /* _ZM_COMMON_H */
