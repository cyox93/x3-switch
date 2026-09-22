add_library(at STATIC
    at_io.c
    at_cmd.c
)

target_include_directories(at PRIVATE .)
target_link_libraries(at PRIVATE core device)
# components/include is added globally in the top-level CMakeLists.
