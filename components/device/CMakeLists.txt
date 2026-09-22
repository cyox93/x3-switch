add_library(device STATIC
    device.c
    dev_key.c
    dev_led.c
    dev_relay.c
    dev_uart.c
)

target_include_directories(device PRIVATE .)
# components/include is added globally in the top-level CMakeLists.

target_link_libraries(device PRIVATE core)
