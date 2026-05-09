cmake_minimum_required(VERSION 3.16)
project(spectre-meltdown-demo
    VERSION 1.0
    DESCRIPTION "Spectre & Meltdown educational PoC"
    LANGUAGES C
)

# ── Compiler requirements ──────────────────────────────────────────────────
# -O0          : prevent constant-folding of array1_size and dead-code
#                elimination of probe_array accesses (the side-channel).
# -fno-inline  : keep victim_function as a real call so the branch predictor
#                sees a branch target, not inlined straight-line code.
# -march=native: expose __rdtscp and _mm_clflush intrinsics.
# These are set per-target (not globally) so they don't infect any future
# subdirectory that might legitimately want optimisation.

if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    message(WARNING "Only GCC and Clang are tested; other compilers may optimise away the gadgets.")
endif()

# ── Build type default ─────────────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING
        "Build type (Debug/Release). Debug keeps -O0 required for the PoC." FORCE)
endif()

# ── Sources ────────────────────────────────────────────────────────────────
add_subdirectory(src)
