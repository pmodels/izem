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

#define zm_ptr_t intptr_t
#define zm_ulong_t unsigned long

#define ZM_NULL (zm_ptr_t)NULL

#if !defined(__STDC_NO_ATOMICS__)

#if defined(_OPENMP)
#define zm_atomic_ptr_t   volatile zm_ptr_t
#define zm_atomic_ulong_t volatile unsigned long
#else
#include <stdatomic.h>
#define zm_atomic_ptr_t   atomic_intptr_t
#define zm_atomic_ulong_t atomic_ulong
#endif

#else
#include <opa_primitives.h>
#endif

#if (__STDC_VERSION__ >= 201112L)
#define zm_thread_local _Thread_local
#endif

#define zm_likely(x)      __builtin_expect(!!(x), 1)
#define zm_unlikely(x)    __builtin_expect(!!(x), 0)

#endif /* _ZM_COMMON_H */
