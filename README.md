# Spectre & Meltdown — Educational PoC

Demonstrates both CPU speculative-execution attacks using the
Flush+Reload cache side-channel.  Written in plain C with x86 intrinsics.

## Requirements

- GCC or Clang
- CMake >= 3.16
- x86-64 CPU with `rdtscp` and `clflush` (`-march=native` detects this)

## Build

```bash
cmake -S . -B build
cmake --build build --parallel
```

The binary lands at `build/src/spectre-meltdown-demo`.

## Run

```bash
./build/src/spectre-meltdown-demo          # both attacks
./build/src/spectre-meltdown-demo spectre  # Spectre v1 only
./build/src/spectre-meltdown-demo meltdown # Meltdown only
```

## Why -O0 -fno-inline are mandatory

Set per-target in `src/CMakeLists.txt`, not globally:

| Flag | Reason |
|------|--------|
| `-O0` | Prevents constant-folding `array1_size` and dead-code elimination of `probe_array` accesses (the side-channel) |
| `-fno-inline` | Keeps `victim_function` as a real call so the branch predictor sees a branch target |
| `-march=native` | Enables `__rdtscp` and `_mm_clflush` intrinsics |

## Project layout

```
.
├── CMakeLists.txt          # top-level: project metadata, build type default
└── src/
    ├── CMakeLists.txt      # per-target compile flags, link graph
    ├── cache.h / cache.c   # probe array, flush_probe_array(), find_cached_slot()
    ├── timing.c            # cached vs uncached latency calibration
    ├── spectre.c           # Spectre v1: bounds-check bypass via BHT training
    ├── meltdown.c          # Meltdown: rogue data load via mprotect simulation
    └── main.c              # argument parsing, sequencing
```

## CMake target graph

```
spectre-meltdown-demo (executable)
├── cache    (static lib) — probe array, flush+reload primitives
├── timing   (static lib) — calibration; links cache
├── spectre  (static lib) — Spectre v1 attack; links cache
└── meltdown (static lib) — Meltdown attack; links cache
```

Each library carries its own target_compile_options so attack-critical flags
are scoped to the PoC code only.

## Why sigaction, not signal

`signal()` leaves SIGSEGV blocked in the process signal mask after the handler
fires. On the second loop iteration the fault raises SIGSEGV again, but it's
masked — the kernel delivers it as SIGKILL. `sigaction` with
`sigemptyset(&sa.sa_mask)` plus reinstalling the handler from inside itself
resets the mask before siglongjmp unwinds the stack.

## Expected output on a patched system

Both attacks report [BLOCKED]. On unpatched pre-2018 Intel hardware, Spectre
returns the correct ASCII bytes and Meltdown reads the protected page.
