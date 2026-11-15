# cmake/gcc-arm-none-eabi.cmake
# Toolchain file for STM32 (Cortex-M3) cross compilation with GCC ARM.
# Style matches PumpModule/Code/cmake/avr-toolchain.cmake.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m3)

set(TOOLCHAIN_PREFIX arm-none-eabi-)

set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_AR           ${TOOLCHAIN_PREFIX}ar)
set(CMAKE_OBJCOPY      ${TOOLCHAIN_PREFIX}objcopy)
set(CMAKE_OBJDUMP      ${TOOLCHAIN_PREFIX}objdump)
set(CMAKE_SIZE         ${TOOLCHAIN_PREFIX}size)
set(CMAKE_RANLIB       ${TOOLCHAIN_PREFIX}ranlib)

# Search programs only outside the target system, libs/headers only inside it
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Bare-metal: no standard system root to link against
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
