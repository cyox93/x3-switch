cmake_minimum_required(VERSION 3.20)

project(x3-switch
    LANGUAGES C ASM
    DESCRIPTION "X3 Switch — MM32F0144C6P I/O controller for X3 Thermostat"
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(MCU_FLAGS
    -mcpu=cortex-m0
    -mthumb
    -mfloat-abi=soft
)

add_compile_options(
    ${MCU_FLAGS}
    -Wall -Wextra -Wshadow
    -ffunction-sections -fdata-sections
    $<$<CONFIG:Debug>:-Og>
    $<$<CONFIG:Debug>:-g3>
    $<$<CONFIG:Release>:-Os>
)

add_link_options(
    ${MCU_FLAGS}
    -Wl,--gc-sections
    -Wl,-Map=${PROJECT_NAME}.map
    -nostartfiles
    --specs=nano.specs
    --specs=nosys.specs
    -T${CMAKE_SOURCE_DIR}/linker/mm32f0144c6p_flash.ld
)

# ---------- Vendor paths ----------
set(SDK_DIR ${CMAKE_SOURCE_DIR}/vendor/mm32f0140/Device)
set(HAL_DIR ${SDK_DIR}/MM32F0140/HAL_Lib)
set(RTT_DIR ${CMAKE_SOURCE_DIR}/vendor/segger_rtt)

# Shared include dirs (vendor SDK + CMSIS + RTT + common headers).
include_directories(
    ${CMAKE_SOURCE_DIR}/components/include
    ${SDK_DIR}/MM32F0140/Include
    ${HAL_DIR}/Inc
    ${SDK_DIR}/CMSIS/KEIL_Core
    ${RTT_DIR}/RTT
    ${RTT_DIR}/Config
)

# ---------- Components ----------
add_subdirectory(components/core)
add_subdirectory(components/device)
add_subdirectory(components/at)

# ---------- Main executable ----------
set(TARGET ${PROJECT_NAME}.elf)

add_executable(${TARGET}
    main/main.c
    main/system_mm32f0140.c
    startup/startup_mm32f0140.s

    # Vendor sources (built directly into the executable, not their own libs)
    ${RTT_DIR}/RTT/SEGGER_RTT.c
    ${RTT_DIR}/RTT/SEGGER_RTT_printf.c
    ${HAL_DIR}/Src/hal_gpio.c
    ${HAL_DIR}/Src/hal_rcc.c
    ${HAL_DIR}/Src/hal_uart.c
    ${HAL_DIR}/Src/hal_flash.c
    ${HAL_DIR}/Src/hal_tim.c
)

target_link_libraries(${TARGET} PRIVATE
    core
    device
    at
)

set_target_properties(${TARGET} PROPERTIES
    LINK_DEPENDS ${CMAKE_SOURCE_DIR}/linker/mm32f0144c6p_flash.ld
)

# ---------- Post-build: size + .bin + .hex ----------
add_custom_command(TARGET ${TARGET} POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${TARGET}>
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${TARGET}> ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${TARGET}> ${PROJECT_NAME}.hex
)
