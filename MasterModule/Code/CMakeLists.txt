cmake_minimum_required(VERSION 4.1.0)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(MasterModule C ASM)

set(TARGET MasterModule)

# ---------------------------------------------------------------- sources --
# Прикладной код и служебные файлы. Новые файлы в Core/Src подхватываются
# автоматически.
file(GLOB APP_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/*.c
)

# Явный список: обновление пакета HAL не должно молча менять состав прошивки.
set(HAL_SOURCES
    stm32f1xx_hal.c
    stm32f1xx_hal_can.c
    stm32f1xx_hal_cortex.c
    stm32f1xx_hal_dma.c
    stm32f1xx_hal_exti.c
    stm32f1xx_hal_flash.c
    stm32f1xx_hal_flash_ex.c
    stm32f1xx_hal_gpio.c
    stm32f1xx_hal_gpio_ex.c
    stm32f1xx_hal_pwr.c
    stm32f1xx_hal_rcc.c
    stm32f1xx_hal_rcc_ex.c
    stm32f1xx_hal_uart.c
)
list(TRANSFORM HAL_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/")

# Векторная таблица
set(ASM_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/startup_stm32f103xb.s)

# ------------------------------------------------------------------ target --
add_executable(${TARGET}
    ${APP_SOURCES}
    ${HAL_SOURCES}
    ${ASM_SOURCES}
)

# -------------------------------------------------------------- definitions --
target_compile_definitions(${TARGET} PRIVATE
    USE_HAL_DRIVER
    STM32F103xB
)

# ---------------------------------------------------------------- includes --
target_include_directories(${TARGET} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Include
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Inc/Legacy
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32F1xx/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Include
)

# ------------------------------------------------------------- compile flags --
target_compile_options(${TARGET} PRIVATE
    -mcpu=cortex-m3
    -mthumb
    -Wall
    -fdata-sections
    -ffunction-sections
    # CMake даёт компилятору абсолютные пути; без этого __FILE__ в assert()
    # попадал бы в прошивку в полном виде. Делает пути относительными
    # (и заодно сборка воспроизводима). Слэш в конце обязателен, иначе
    # останется ведущий '/'.
    -ffile-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}/=
    $<$<CONFIG:Debug>:-Og -g -gdwarf-2>
    $<$<CONFIG:Release>:-Os>
)

# ------------------------------------------------------------------ linking --
set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/STM32F103XX_FLASH.ld)

# Флаги линковки: CMake помещает их ДО объектных файлов.
target_link_options(${TARGET} PRIVATE
    -mcpu=cortex-m3
    -mthumb
    -specs=nano.specs
    -T${LINKER_SCRIPT}
    -Wl,--gc-sections
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.map,--cref
)

# Библиотеки: target_link_libraries попадает ПОСЛЕ объектных файлов,
# иначе -lnosys конфликтует с _exit/_close/_sbrk из Core/Src/syscalls.c.
# Порядок важен: c -> m -> nosys.
target_link_libraries(${TARGET} PRIVATE c m nosys)

set_target_properties(${TARGET} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    SUFFIX ".elf"
    C_STANDARD 11
    C_STANDARD_REQUIRED YES
    # FIFO использует арифметику void*: пока оставляем расширения GNU C.
    C_EXTENSIONS YES
    LINK_DEPENDS "${LINKER_SCRIPT}"
)

# ------------------------------------------------------------- post-processing --
# HEX и BIN из ELF
set(FLASH_BIN "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.bin")
set(FLASH_HEX "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.hex")
add_custom_command(TARGET ${TARGET} POST_BUILD
    BYPRODUCTS "${FLASH_HEX}" "${FLASH_BIN}"
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${TARGET}> "${FLASH_HEX}"
    COMMAND ${CMAKE_OBJCOPY} -O binary -S $<TARGET_FILE:${TARGET}> "${FLASH_BIN}"
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${TARGET}>
    COMMENT "Generating HEX/BIN and printing size"
    VERBATIM
)

# -------------------------------------------------------------------- flash --
# st-flash из stlink-утилит
set(FLASH_ADDRESS 0x08000000)

add_custom_target(flash
    COMMAND st-flash --connect-under-reset write "${FLASH_BIN}" ${FLASH_ADDRESS}
    DEPENDS ${TARGET}
    COMMENT "Flashing ${FLASH_BIN} to ${FLASH_ADDRESS}"
    VERBATIM
)

add_custom_target(erase
    COMMAND st-flash --connect-under-reset erase
    COMMENT "Erasing flash"
    VERBATIM
)

add_custom_target(ram
    COMMAND st-info --probe
    COMMENT "Probing ST-Link"
    VERBATIM
)
