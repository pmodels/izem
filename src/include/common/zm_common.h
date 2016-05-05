#ifndef _ZM_UTIL_H
#define _ZM_UTIL_H

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

#endif /* _ZM_UTIL_H */
