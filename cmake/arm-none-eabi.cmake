# ARM bare-metal toolchain file for MM32F0144C6P (Cortex-M0)

set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(TOOLCHAIN_PREFIX       arm-none-eabi-)
set(CMAKE_C_COMPILER       ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_ASM_COMPILER     ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER     ${TOOLCHAIN_PREFIX}g++)
set(CMAKE_OBJCOPY          ${TOOLCHAIN_PREFIX}objcopy CACHE INTERNAL "")
set(CMAKE_OBJDUMP          ${TOOLCHAIN_PREFIX}objdump CACHE INTERNAL "")
set(CMAKE_SIZE             ${TOOLCHAIN_PREFIX}size    CACHE INTERNAL "")

# Bare-metal cross-compile: we can't link a test executable without our linker
# script, so tell CMake to test compilation by producing a static library.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
