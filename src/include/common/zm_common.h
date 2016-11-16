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
#include <stdint.h>
#define zm_ptr_t intptr_t
#define zm_ulong_t unsigned long

#define ZM_NULL (zm_ptr_t)NULL

/* FIXME Test for C11 atomic types and functions in configure. */
#if !defined(__STDC_NO_ATOMICS__)

#if defined(_OPENMP)
#define zm_atomic_uint_t  volatile unsigned int
#define zm_atomic_ptr_t   volatile zm_ptr_t
#define zm_atomic_ulong_t volatile unsigned long
#else
#include <stdatomic.h>
#define zm_atomic_uint_t  atomic_uint
#define zm_atomic_ptr_t   atomic_intptr_t
#define zm_atomic_ulong_t atomic_ulong
#endif

#else
#include <opa_primitives.h>
#endif

/* FIXME It would be better to test for the presence of a TLS keyword in configure. */
#if (__STDC_VERSION__ >= 201112L)
#define zm_thread_local _Thread_local
#elif defined(__GNUC__) && (__GNUC__ > 3)
/* __thread was introduced in GCC 3.3.1 (https://gcc.gnu.org/onlinedocs/gcc-3.3.1/gcc/C99-Thread-Local-Edits.html) */
#define zm_thread_local __thread
#endif

#define zm_likely(x)      __builtin_expect(!!(x), 1)
#define zm_unlikely(x)    __builtin_expect(!!(x), 0)

#endif /* _ZM_COMMON_H */
