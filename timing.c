#include "timing.h"
#include "cache.h"
#include <stdio.h>
#include <stdint.h>
#include <x86intrin.h>

/* Measure median access time for a cached vs uncached byte.
   Prints a small calibration table and returns 0. */
int calibrate_cache_timing(void) {
    printf("  Cache timing calibration\n");
    printf("  ────────────────────────\n");

    volatile uint8_t *slot = &probe_array[0];

    /* --- Cached access --- */
    (void)*slot;   /* warm it up */
    uint64_t cached_total = 0;
    for (int i = 0; i < 100; i++) {
        uint32_t junk;
        uint64_t t1 = __rdtscp(&junk);
        (void)*slot;
        uint64_t t2 = __rdtscp(&junk);
        cached_total += (t2 - t1);
    }

    /* --- Uncached access --- */
    uint64_t uncached_total = 0;
    for (int i = 0; i < 100; i++) {
        _mm_clflush((void *)slot);
        _mm_mfence();
        uint32_t junk;
        uint64_t t1 = __rdtscp(&junk);
        (void)*slot;
        uint64_t t2 = __rdtscp(&junk);
        uncached_total += (t2 - t1);
    }

    printf("  Cached   read : %4llu cycles (avg over 100 reads)\n",
           (unsigned long long)(cached_total / 100));
    printf("  Uncached read : %4llu cycles (avg over 100 reads)\n",
           (unsigned long long)(uncached_total / 100));
    printf("  Hit threshold : %4d cycles (CACHE_HIT_NS constant)\n\n",
           CACHE_HIT_NS);

    return 0;
}
