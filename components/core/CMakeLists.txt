add_library(core STATIC
    log.c
    clock.c
    tick.c
)

target_include_directories(core PRIVATE .)
