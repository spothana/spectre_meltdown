#include "cache.h"
#include <stdint.h>

/* The probe array must be large enough that each of the 256 possible byte
   values maps to its own cache line.  We over-allocate by one slot so that
   even slot 255 doesn't share a line with anything else. */
uint8_t probe_array[NUM_SLOTS * CACHE_STRIDE];

int find_cached_slot(uint64_t *times) {
    int best = 0;
    uint64_t best_time = UINT64_MAX;

    for (int i = 0; i < NUM_SLOTS; i++) {
        uint64_t t = probe_slot(i);
        if (times) times[i] = t;
        if (t < best_time) {
            best_time = t;
            best = i;
        }
    }
    return best;
}
