#include <stdio.h>
#include <string.h>
#include "cache.h"
#include "timing.h"
#include "spectre.h"
#include "meltdown.h"

static void print_banner(void) {
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("  Spectre & Meltdown  —  educational PoC\n");
    printf("  Demonstrates CPU speculative-execution side-channels\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n");
}

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s [spectre|meltdown|all]\n", prog);
    fprintf(stderr, "  spectre   — Spectre variant 1 (bounds-check bypass)\n");
    fprintf(stderr, "  meltdown  — Meltdown (rogue data cache load)\n");
    fprintf(stderr, "  all       — run both (default)\n");
}

int main(int argc, char *argv[]) {
    const char *mode = "all";
    if (argc == 2) {
        mode = argv[1];
        if (strcmp(mode, "spectre")  != 0 &&
            strcmp(mode, "meltdown") != 0 &&
            strcmp(mode, "all")      != 0) {
            usage(argv[0]);
            return 1;
        }
    } else if (argc > 2) {
        usage(argv[0]);
        return 1;
    }

    print_banner();

    printf("Calibrating...\n");
    calibrate_cache_timing();

    if (strcmp(mode, "spectre") == 0 || strcmp(mode, "all") == 0)
        spectre_demo();

    if (strcmp(mode, "meltdown") == 0 || strcmp(mode, "all") == 0)
        meltdown_demo();

    printf("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("  Done.\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    return 0;
}
