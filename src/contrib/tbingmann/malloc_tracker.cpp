/*******************************************************************************
 * thrill/mem/malloc_tracker.cpp
 *
 * Part of Project Thrill - http://project-thrill.org
 *
 * Copyright (C) 2013-2016 Timo Bingmann <tb@panthema.net>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "malloc_tracker.hpp"

#if defined(__GNUC__) || defined(__clang__)
#define TLX_LIKELY(c)   __builtin_expect((c), 1)
#define TLX_UNLIKELY(c) __builtin_expect((c), 0)
#else
#define TLX_LIKELY(c)   c
#define TLX_UNLIKELY(c) c
#endif

#if __linux__ || __APPLE__ || __FreeBSD__

#include <dlfcn.h>

#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>
#include <utility>

#if defined(__clang__) || defined(__GNUC__)

#define ATTRIBUTE_NO_SANITIZE_ADDRESS \
    __attribute__ ((no_sanitize_address)) /* NOLINT */

#if defined(__GNUC__) && __GNUC__ >= 5
#define ATTRIBUTE_NO_SANITIZE_THREAD \
    __attribute__ ((no_sanitize_thread))  /* NOLINT */
#else
#define ATTRIBUTE_NO_SANITIZE_THREAD
#endif

#define ATTRIBUTE_NO_SANITIZE \
    ATTRIBUTE_NO_SANITIZE_ADDRESS ATTRIBUTE_NO_SANITIZE_THREAD

#else
#define ATTRIBUTE_NO_SANITIZE
#endif

namespace thrill {
namespace mem {

/******************************************************************************/
// variables of malloc tracker

//! In the generic hook implementation, we add to each allocation additional
//! data for bookkeeping.
static constexpr size_t padding = 16;    /* bytes (>= 2*sizeof(size_t)) */

//! function pointer to the real procedures, loaded using dlsym()
using malloc_type = void* (*)(size_t);
using free_type = void (*)(void*);
using realloc_type = void* (*)(void*, size_t);
using aligned_alloc_type = void* (*)(size_t, size_t);

static malloc_type real_malloc = nullptr;
static free_type real_free = nullptr;
static realloc_type real_realloc = nullptr;
static aligned_alloc_type real_aligned_alloc = nullptr;

//! a sentinel value prefixed to each allocation
static constexpr size_t sentinel = 0xDEADC0DE;

#define USE_ATOMICS 0

//! CounterType is used for atomic counters, and get to retrieve their
//! contents. Due to the thread-local cached statistics, the overall memory
//! usage counter can actually go negative!
#if defined(_MSC_VER) || USE_ATOMICS
using CounterType = std::atomic<ssize_t>;
#else
// we cannot use std::atomic on gcc/clang because only real atomic instructions
// work with the Sanitizers
using CounterType = ssize_t;
#endif

// actually only such that formatting is not messed up
#define COUNTER_ZERO { 0 }

ATTRIBUTE_NO_SANITIZE
static inline ssize_t get(const CounterType& a) {
#if defined(_MSC_VER) || USE_ATOMICS
    return a.load();
#else
    return a;
#endif
}

ATTRIBUTE_NO_SANITIZE
static inline ssize_t sync_add_and_fetch(CounterType& curr, ssize_t inc) {
#if defined(_MSC_VER) || USE_ATOMICS
    return (curr += inc);
#else
    return __sync_add_and_fetch(&curr, inc);
#endif
}

ATTRIBUTE_NO_SANITIZE
static inline ssize_t sync_sub_and_fetch(CounterType& curr, ssize_t dec) {
#if defined(_MSC_VER) || USE_ATOMICS
    return (curr -= dec);
#else
    return __sync_sub_and_fetch(&curr, dec);
#endif
}

//! a simple memory heap for allocations prior to dlsym loading
#define INIT_HEAP_SIZE 1024 * 1024
static char init_heap[INIT_HEAP_SIZE];
static CounterType init_heap_use COUNTER_ZERO;
static constexpr int log_operations_init_heap = 0;

//! align allocations to init_heap to this number by rounding up allocations
static constexpr size_t init_alignment = sizeof(size_t);

//! output
#define PPREFIX "malloc_tracker ### "

/******************************************************************************/
// Run-time memory allocation statistics

static CounterType total_allocs COUNTER_ZERO;
static CounterType current_allocs COUNTER_ZERO;

static CounterType total_bytes COUNTER_ZERO;
static CounterType peak_bytes COUNTER_ZERO;

// free-floating memory allocated by malloc/free
static CounterType float_curr COUNTER_ZERO;

// Thrill base memory allocated by bypass_malloc/bypass_free
static CounterType base_curr COUNTER_ZERO;

//! memory limit exceeded indicator
bool memory_exceeded = false;
ssize_t memory_limit_indication = std::numeric_limits<ssize_t>::max();

struct LocalStats {
    size_t  total_allocs;
    int64_t current_allocs;
    int64_t bytes;
};

#if !defined(__APPLE__)
#define HAVE_THREAD_LOCAL 1
#else
#define HAVE_THREAD_LOCAL 0
#endif

#if HAVE_THREAD_LOCAL
static thread_local LocalStats tl_stats = { 0, 0, 0 };
static const ssize_t tl_delay_threshold = 1024 * 1024;
#endif

ATTRIBUTE_NO_SANITIZE
void update_peak(ssize_t float_curr_new, ssize_t base_curr_new) {
    if (float_curr_new + base_curr_new > peak_bytes)
        peak_bytes = float_curr_new + base_curr_new;
}

ATTRIBUTE_NO_SANITIZE
void flush_memory_statistics() {
#if HAVE_THREAD_LOCAL
    // no-operation of no thread_local is available.
    ssize_t mycurr = sync_add_and_fetch(float_curr, tl_stats.bytes);

    sync_add_and_fetch(total_bytes, tl_stats.bytes);
    sync_add_and_fetch(total_allocs, tl_stats.total_allocs);
    sync_add_and_fetch(current_allocs, tl_stats.current_allocs);
    update_peak(mycurr, base_curr);

    memory_exceeded = (mycurr >= memory_limit_indication);

    tl_stats.bytes = 0;
    tl_stats.total_allocs = 0;
    tl_stats.current_allocs = 0;
#endif
}

//! add allocation to statistics
ATTRIBUTE_NO_SANITIZE
static void inc_count(size_t inc) {
#if HAVE_THREAD_LOCAL
    tl_stats.total_allocs++;
    tl_stats.current_allocs++;
    tl_stats.bytes += inc;

    if (tl_stats.bytes > tl_delay_threshold)
        flush_memory_statistics();
#else
    // no thread_local data structure -> update immediately (more contention)
    ssize_t mycurr = sync_add_and_fetch(float_curr, inc);
    total_bytes += inc;
    update_peak(mycurr, base_curr);

    sync_add_and_fetch(total_allocs, 1);
    sync_add_and_fetch(current_allocs, 1);

    memory_exceeded = (mycurr >= memory_limit_indication);
#endif
}

//! decrement allocation to statistics
ATTRIBUTE_NO_SANITIZE
static void dec_count(size_t dec) {
#if HAVE_THREAD_LOCAL
    tl_stats.current_allocs--;
    tl_stats.bytes -= dec;

    if (tl_stats.bytes < -tl_delay_threshold)
        flush_memory_statistics();
#else
    // no thread_local data structure -> update immediately (more contention)
    ssize_t mycurr = sync_sub_and_fetch(float_curr, dec);

    sync_sub_and_fetch(current_allocs, 1);

    memory_exceeded = (mycurr >= memory_limit_indication);
#endif
}

//! user function to return the currently allocated amount of memory
ssize_t malloc_tracker_current() {
    return float_curr;
}

//! user function to return the peak allocation
ssize_t malloc_tracker_peak() {
    return peak_bytes;
}

//! user function to reset the peak allocation to current
void malloc_tracker_reset_peak() {
    peak_bytes = get(float_curr);
}

//! user function to return total number of allocations
ssize_t malloc_tracker_total_allocs() {
    return total_allocs;
}

//! user function which prints current and peak allocation to stderr
void malloc_tracker_print_status() {
    fprintf(stderr, PPREFIX "floating %zu, peak %zu, base %zu\n",
            get(float_curr), get(peak_bytes), get(base_curr));
}

void set_memory_limit_indication(ssize_t size) {
    // fprintf(stderr, PPREFIX "set_memory_limit_indication %zu\n", size);
    memory_limit_indication = size;
}

/******************************************************************************/
// Initialize function pointers to the real underlying malloc implementation.

#if __linux__ || __APPLE__ || __FreeBSD__

ATTRIBUTE_NO_SANITIZE
static __attribute__ ((constructor)) void init() { // NOLINT

    // try to use AddressSanitizer's malloc first.
    real_malloc = (malloc_type)dlsym(RTLD_DEFAULT, "__interceptor_malloc");
    if (real_malloc)
    {
        real_realloc = (realloc_type)dlsym(RTLD_DEFAULT, "__interceptor_realloc");
        if (!real_realloc) {
            fprintf(stderr, PPREFIX "dlerror %s\n", dlerror());
            exit(EXIT_FAILURE);
        }

        real_free = (free_type)dlsym(RTLD_DEFAULT, "__interceptor_free");
        if (!real_free) {
            fprintf(stderr, PPREFIX "dlerror %s\n", dlerror());
            exit(EXIT_FAILURE);
        }

        fprintf(stderr, PPREFIX "using AddressSanitizer's malloc\n");

        return;
    }

    real_malloc = (malloc_type)dlsym(RTLD_NEXT, "malloc");
    if (!real_malloc) {
        fprintf(stderr, PPREFIX "dlerror %s\n", dlerror());
        exit(EXIT_FAILURE);
    }

    real_realloc = (realloc_type)dlsym(RTLD_NEXT, "realloc");
    if (!real_realloc) {
        fprintf(stderr, PPREFIX "dlerror %s\n", dlerror());
        exit(EXIT_FAILURE);
    }

    real_aligned_alloc = (aligned_alloc_type)dlsym(RTLD_NEXT, "aligned_alloc");

    real_free = (free_type)dlsym(RTLD_NEXT, "free");
    if (!real_free) {
        fprintf(stderr, PPREFIX "dlerror %s\n", dlerror());
        exit(EXIT_FAILURE);
    }
}

ATTRIBUTE_NO_SANITIZE
static __attribute__ ((destructor)) void finish() { // NOLINT
    fprintf(stderr, PPREFIX
            "exiting, total: %zu, peak: %zu, current: %zu / %zu, "
            "allocs: %zu, unfreed: %zu\n",
            get(total_bytes), get(peak_bytes),
            get(float_curr), get(base_curr),
            get(total_allocs), get(current_allocs));
}

#endif

/******************************************************************************/
// Functions to bypass the malloc tracker

ATTRIBUTE_NO_SANITIZE
void * bypass_malloc(size_t size) noexcept {
#if defined(_MSC_VER)
    void* ptr = malloc(size);
#else
    void* ptr = real_malloc(size);
#endif
    if (!ptr) {
        fprintf(stderr, PPREFIX "bypass_malloc(%zu size) = %p   (current %zu / %zu)\n",
                size, ptr, get(float_curr), get(base_curr));
        return ptr;
    }

    ssize_t mycurr = sync_add_and_fetch(base_curr, size);

    total_bytes += size;
    update_peak(float_curr, mycurr);

    sync_add_and_fetch(total_allocs, 1);
    sync_add_and_fetch(current_allocs, 1);

    return ptr;
}

ATTRIBUTE_NO_SANITIZE
void bypass_free(void* ptr, size_t size) noexcept {

    sync_sub_and_fetch(base_curr, size);
    sync_sub_and_fetch(current_allocs, 1);

#if defined(_MSC_VER)
    return free(ptr);
#else
    return real_free(ptr);
#endif
}

ATTRIBUTE_NO_SANITIZE
void * bypass_aligned_alloc(size_t alignment, size_t size) noexcept {
#if defined(_MSC_VER)
    void* ptr = _aligned_malloc(size, alignment);
#else
    void* ptr;
    if (real_aligned_alloc) {
        ptr = real_aligned_alloc(alignment, size);
    }
    else {
        // emulate alignment by wasting memory
        void* mem = real_malloc((alignment - 1) + sizeof(void*) + size);

        uintptr_t uptr = reinterpret_cast<uintptr_t>(mem) + sizeof(void*);
        uptr += alignment - (uptr & (alignment - 1));
        ptr = reinterpret_cast<void*>(uptr);

        // store original pointer for deallocation
        (reinterpret_cast<void**>(ptr))[-1] = mem;
    }
#endif
    if (!ptr) {
        fprintf(stderr, PPREFIX "bypass_aligned_alloc(%zu align %zu size) = %p   (current %zu / %zu)\n",
                alignment, size, ptr, get(float_curr), get(base_curr));
        return ptr;
    }

    ssize_t mycurr = sync_add_and_fetch(base_curr, size);

    total_bytes += size;
    update_peak(float_curr, mycurr);

    sync_add_and_fetch(total_allocs, 1);
    sync_add_and_fetch(current_allocs, 1);

    return ptr;
}

ATTRIBUTE_NO_SANITIZE
void bypass_aligned_free(void* ptr, size_t size) noexcept {

    sync_sub_and_fetch(base_curr, size);
    sync_sub_and_fetch(current_allocs, 1);

#if defined(_MSC_VER)
    return _aligned_free(ptr);
#else
    if (real_aligned_alloc) {
        return real_free(ptr);
    }
    else {
        real_free((reinterpret_cast<void**>(ptr))[-1]);
    }
#endif
}

} // namespace mem
} // namespace thrill

/******************************************************************************/
// exported symbols that overlay the libc functions

using namespace thrill::mem; // NOLINT

ATTRIBUTE_NO_SANITIZE
static void * preinit_malloc(size_t size) noexcept {

    size_t aligned_size = size + (init_alignment - size % init_alignment);

#if defined(_MSC_VER) || USE_ATOMICS
    size_t offset = (init_heap_use += (padding + aligned_size));
#else
    size_t offset = __sync_fetch_and_add(&init_heap_use, padding + aligned_size);
#endif

    if (offset > INIT_HEAP_SIZE) {
        fprintf(stderr, PPREFIX "init heap full !!!\n");
        exit(EXIT_FAILURE);
    }

    char* ret = init_heap + offset;

    //! prepend allocation size and check sentinel
    *reinterpret_cast<size_t*>(ret) = aligned_size;
    *reinterpret_cast<size_t*>(ret + padding - sizeof(size_t)) = sentinel;

    inc_count(aligned_size);

    if (log_operations_init_heap) {
        fprintf(stderr, PPREFIX "malloc(%zu / %zu) = %p   on init heap\n",
                size, aligned_size, static_cast<void*>(ret + padding));
    }

    return ret + padding;
}

ATTRIBUTE_NO_SANITIZE
static void * preinit_realloc(void* ptr, size_t size) {

    if (log_operations_init_heap) {
        fprintf(stderr, PPREFIX "realloc(%p) = on init heap\n", ptr);
    }

    ptr = static_cast<char*>(ptr) - padding;

    if (*reinterpret_cast<size_t*>(
            static_cast<char*>(ptr) + padding - sizeof(size_t)) != sentinel) {
        fprintf(stderr, PPREFIX
                "realloc(%p) has no sentinel !!! memory corruption?\n",
                ptr);
    }

    size_t oldsize = *reinterpret_cast<size_t*>(ptr);

    if (oldsize >= size) {
        //! keep old area
        return static_cast<char*>(ptr) + padding;
    }
    else {
        //! allocate new area and copy data
        ptr = static_cast<char*>(ptr) + padding;
        void* newptr = malloc(size);
        memcpy(newptr, ptr, oldsize);
        free(ptr);
        return newptr;
    }
}

ATTRIBUTE_NO_SANITIZE
static void preinit_free(void* ptr) {
    // don't do any real deallocation.

    ptr = static_cast<char*>(ptr) - padding;

    if (*reinterpret_cast<size_t*>(
            static_cast<char*>(ptr) + padding - sizeof(size_t)) != sentinel) {
        fprintf(stderr, PPREFIX
                "free(%p) has no sentinel !!! memory corruption?\n",
                ptr);
    }

    size_t size = *reinterpret_cast<size_t*>(ptr);
    dec_count(size);

    if (log_operations_init_heap) {
        fprintf(stderr, PPREFIX "free(%p) -> %zu   on init heap\n", ptr, size);
    }
}

#if defined(__APPLE__) && __APPLE__

#define NOEXCEPT
#define MALLOC_USABLE_SIZE malloc_size
#include <malloc/malloc.h>

#elif defined(__FreeBSD__) && __FreeBSD__

#define NOEXCEPT
#define MALLOC_USABLE_SIZE malloc_usable_size
#include <malloc_np.h>

#elif defined(__linux__) && __linux__

#define NOEXCEPT noexcept
#define MALLOC_USABLE_SIZE malloc_usable_size
#include <malloc.h>

#endif


/******************************************************************************/

#if defined(MALLOC_USABLE_SIZE)

/*
 * This is a malloc() tracker implementation which uses an available system call
 * to determine the amount of memory used by an allocation (which may be more
 * than the allocated size). On Linux's glibc there is malloc_usable_size().
 */

//! exported malloc symbol that overrides loading from libc
ATTRIBUTE_NO_SANITIZE
void * malloc(size_t size) NOEXCEPT {

    if (TLX_UNLIKELY(!real_malloc))
        return preinit_malloc(size);

    //! call real malloc procedure in libc
    void* ret = (*real_malloc)(size);
    if (!ret) {
        fprintf(stderr, PPREFIX "malloc(%zu size) = %p   (current %zu / %zu)\n",
                size, ret, get(float_curr), get(base_curr));
        return nullptr;
    }

    size_t size_used = MALLOC_USABLE_SIZE(ret);
    inc_count(size_used);

    return ret;
}

//! exported free symbol that overrides loading from libc
ATTRIBUTE_NO_SANITIZE
void free(void* ptr) NOEXCEPT {

    if (!ptr) return;   //! free(nullptr) is no operation

    if (TLX_UNLIKELY(
            static_cast<char*>(ptr) >= init_heap &&
            static_cast<char*>(ptr) <= init_heap + get(init_heap_use)))
    {
        return preinit_free(ptr);
    }

    if (TLX_UNLIKELY(!real_free)) {
        fprintf(stderr, PPREFIX
                "free(%p) outside init heap and without real_free !!!\n", ptr);
        return;
    }

    size_t size_used = MALLOC_USABLE_SIZE(ptr);
    dec_count(size_used);

    (*real_free)(ptr);
}

//! exported calloc() symbol that overrides loading from libc, implemented using
//! our malloc
ATTRIBUTE_NO_SANITIZE
void * calloc(size_t nmemb, size_t size) NOEXCEPT {
    size *= nmemb;
    void* ret = malloc(size);
    if (!ret) return ret;
    memset(ret, 0, size);
    return ret;
}

//! exported realloc() symbol that overrides loading from libc
ATTRIBUTE_NO_SANITIZE
void * realloc(void* ptr, size_t size) NOEXCEPT {

    if (static_cast<char*>(ptr) >= static_cast<char*>(init_heap) &&
        static_cast<char*>(ptr) <= static_cast<char*>(init_heap) + get(init_heap_use))
    {
        return preinit_realloc(ptr, size);
    }

    if (size == 0) { //! special case size == 0 -> free()
        free(ptr);
        return nullptr;
    }

    if (ptr == nullptr) { //! special case ptr == 0 -> malloc()
        return malloc(size);
    }

    size_t oldsize_used = MALLOC_USABLE_SIZE(ptr);
    dec_count(oldsize_used);

    void* newptr = (*real_realloc)(ptr, size);
    if (!newptr) return nullptr;

    size_t newsize_used = MALLOC_USABLE_SIZE(newptr);
    inc_count(newsize_used);

    return newptr;
}

/******************************************************************************/

#elif !defined(_MSC_VER) // GENERIC IMPLEMENTATION for Unix

/*
 * This is a generic implementation to count memory allocation by prefixing
 * every user allocation with the size. On free, the size can be
 * retrieves. Obviously, this wastes lots of memory if there are many small
 * allocations.
 */

//! exported malloc symbol that overrides loading from libc
ATTRIBUTE_NO_SANITIZE
void * malloc(size_t size) NOEXCEPT {

    if (!real_malloc)
        return preinit_malloc(size);

    //! call real malloc procedure in libc
    void* ret = (*real_malloc)(padding + size);

    inc_count(size);

    //! prepend allocation size and check sentinel
    *reinterpret_cast<size_t*>(ret) = size;
    *reinterpret_cast<size_t*>(
        static_cast<char*>(ret) + padding - sizeof(size_t)) = sentinel;

    return static_cast<char*>(ret) + padding;
}

//! exported free symbol that overrides loading from libc
ATTRIBUTE_NO_SANITIZE
void free(void* ptr) NOEXCEPT {

    if (!ptr) return;   //! free(nullptr) is no operation

    if (static_cast<char*>(ptr) >= init_heap &&
        static_cast<char*>(ptr) <= init_heap + get(init_heap_use))
    {
        return preinit_free(ptr);
    }

    if (!real_free) {
        fprintf(stderr, PPREFIX
                "free(%p) outside init heap and without real_free !!!\n", ptr);
        return;
    }

    ptr = static_cast<char*>(ptr) - padding;

    if (*reinterpret_cast<size_t*>(
            static_cast<char*>(ptr) + padding - sizeof(size_t)) != sentinel) {
        fprintf(stderr, PPREFIX
                "free(%p) has no sentinel !!! memory corruption?\n", ptr);
    }

    size_t size = *reinterpret_cast<size_t*>(ptr);
    dec_count(size);

    (*real_free)(ptr);
}

//! exported calloc() symbol that overrides loading from libc, implemented using
//! our malloc
ATTRIBUTE_NO_SANITIZE
void * calloc(size_t nmemb, size_t size) NOEXCEPT {
    size *= nmemb;
    if (!size) return nullptr;
    void* ret = malloc(size);
    if (!ret) return ret;
    memset(ret, 0, size);
    return ret;
}

//! exported realloc() symbol that overrides loading from libc
ATTRIBUTE_NO_SANITIZE
void * realloc(void* ptr, size_t size) NOEXCEPT {

    if (static_cast<char*>(ptr) >= static_cast<char*>(init_heap) &&
        static_cast<char*>(ptr) <=
        static_cast<char*>(init_heap) + get(init_heap_use))
    {
        return preinit_realloc(ptr, size);
    }

    if (size == 0) { //! special case size == 0 -> free()
        free(ptr);
        return nullptr;
    }

    if (ptr == nullptr) { //! special case ptr == 0 -> malloc()
        return malloc(size);
    }

    ptr = static_cast<char*>(ptr) - padding;

    if (*reinterpret_cast<size_t*>(
            static_cast<char*>(ptr) + padding - sizeof(size_t)) != sentinel) {
        fprintf(stderr, PPREFIX
                "free(%p) has no sentinel !!! memory corruption?\n", ptr);
    }

    size_t oldsize = *reinterpret_cast<size_t*>(ptr);

    dec_count(oldsize);
    inc_count(size);

    void* newptr = (*real_realloc)(ptr, padding + size);

    *reinterpret_cast<size_t*>(newptr) = size;

    return static_cast<char*>(newptr) + padding;
}

/******************************************************************************/

#else // if defined(_MSC_VER)

// TODO(tb): dont know how to override malloc/free.

#endif // IMPLEMENTATION SWITCH

/******************************************************************************/
