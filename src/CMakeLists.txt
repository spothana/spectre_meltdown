# ── cache (static library shared by spectre and meltdown) ─────────────────
add_library(cache STATIC
    cache.c
)

target_include_directories(cache PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(cache PRIVATE
    -O0
    -fno-inline
    -march=native
    -Wall
    -Wextra
)

# ── timing (static library) ────────────────────────────────────────────────
add_library(timing STATIC
    timing.c
)

target_include_directories(timing PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(timing PUBLIC cache)

target_compile_options(timing PRIVATE
    -O0
    -fno-inline
    -march=native
    -Wall
    -Wextra
)

# ── spectre (static library) ───────────────────────────────────────────────
add_library(spectre STATIC
    spectre.c
)

target_include_directories(spectre PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(spectre PUBLIC cache)

target_compile_options(spectre PRIVATE
    -O0
    -fno-inline
    -march=native
    -Wall
    -Wextra
)

# ── meltdown (static library) ──────────────────────────────────────────────
add_library(meltdown STATIC
    meltdown.c
)

target_include_directories(meltdown PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(meltdown PUBLIC cache)

target_compile_options(meltdown PRIVATE
    -O0
    -fno-inline
    -march=native
    -Wall
    -Wextra
    # _POSIX_C_SOURCE for sigjmp_buf / sigsetjmp / siglongjmp
    -D_POSIX_C_SOURCE=200112L
)

# ── main executable ────────────────────────────────────────────────────────
add_executable(spectre-meltdown-demo
    main.c
)

target_compile_options(spectre-meltdown-demo PRIVATE
    -O0
    -march=native
    -Wall
    -Wextra
)

target_link_libraries(spectre-meltdown-demo PRIVATE
    cache
    timing
    spectre
    meltdown
)

# ── install rules (optional) ───────────────────────────────────────────────
install(TARGETS spectre-meltdown-demo
    RUNTIME DESTINATION bin
)
