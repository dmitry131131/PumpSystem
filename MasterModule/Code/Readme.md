# Master Module

Pipeline:
Configuration (Core/Inc/BoardConfig.h) -> Arm-GCC + CMake build -> st-flash upload

Настройки тактирования, CAN/UART, пинов и NVIC: `Core/Inc/BoardConfig.h`.
Частота кварца: `Core/Inc/BoardClockConfig.h` — общий заголовок без HAL,
используемый также HAL и CMSIS. CubeMX для настройки и сборки не требуется.

Разводка фиксирована под MasterModule PCB: LED PC13, EXTI PB11,
CAN1 PB8/PB9 (remap 2), USART1 PA9/PA10 (без remap).
Статические проверки не разрешат поменять только макрос пина: для другой
разводки нужно согласованно изменить BoardConfig, Board и обработчики IRQ.
Числовые параметры PLL, делителей шин и CAN вычисляются из констант HAL.
Проверяются допустимые частоты, Flash latency (питание 3.3 V), тайминг CAN,
UART и приоритеты NVIC. Текущие частоты сохранены: SYSCLK 72 МГц,
HCLK/PCLK1/PCLK2 36 МГц, CAN 50 кбит/с, UART 115200 8N1.
Автоматический повтор передачи CAN включён (`BOARD_CAN_AUTO_RETRANSMISSION`).

Зависимости: CMake >= 4.1, Ninja, Arm GNU Toolchain с newlib-nano;
для прошивки — stlink (`st-flash`, `st-info`). Сборка проверена с CMake 4.4.4,
Ninja 1.13.2 и arm-none-eabi-gcc 16.2.0. Стандарт исходников — GNU C11:
расширения GNU пока нужны существующей реализации FIFO.

В `Drivers` хранится используемый набор исходников STMicroelectronics,
оставшийся от проекта CubeMX, без загрузки зависимостей при сборке:
STM32F1 HAL 1.1.10 (`stm32f1xx_hal.c`), CMSIS Device STM32F1 4.3.5
(`stm32f1xx.h`), CMSIS Core 5.1 (`cmsis_version.h`). Это версии из самих файлов;
исходный тег пакета STM32CubeF1 не зафиксирован. Лицензии сохранены в `Drivers`.
При обновлении сохраняйте совместимость HAL/CMSIS и явно обновляйте список
HAL-исходников в CMake и включённых модулей в `stm32f1xx_hal_conf.h`.

# Build
```bash
    cd MasterModule/Code/
    cmake --preset Debug
    cmake --build --preset Debug
    cmake --preset Release
    cmake --build --preset Release
```
Артефакты: `build/Debug/MasterModule.{elf,hex,bin}` + карта `MasterModule.map`.
Для Release — те же имена в `build/Release`. Изменение linker script
автоматически вызывает перелинковку; отсутствующие HEX/BIN создаются повторно.
Общие заголовки протокола берутся из общего каталога `../../Include`, поэтому при
копировании проекта необходимо сохранять структуру репозитория PumpSystem.

# Load
```bash
    st-info --probe
    st-flash write ./build/Debug/MasterModule.bin 0x08000000
    st-flash --connect-under-reset write ./build/Debug/MasterModule.bin 0x08000000
```
По умолчанию `BOARD_SWJ_CONFIG = AFIO_MAPR_SWJ_CFG_RESET`: JTAG и SWD включены.
Чтобы освободить PA15/PB3/PB4, сохранив SWD, выберите
`AFIO_MAPR_SWJ_CFG_JTAGDISABLE`. Полное отключение — `AFIO_MAPR_SWJ_CFG_DISABLE`.
Режим SWJ задаётся явно и при remap периферии.
`--connect-under-reset` оставлен в CMake для восстановления связи в том числе
после старой прошивки с отключённым SWD; для него подключите NRST программатора.

То же самое через цель CMake:
```bash
    cmake --build --preset Debug --target flash
    cmake --build --preset Debug --target erase
```

# Erase
```bash
    st-flash --connect-under-reset erase
```

# COM port debug

```bash
    minicom -D /dev/ttyUSB0 -b 115200
```

# Разъем ST-link - Разъем SWD
3,3V ---> 1pin
GND ---> 10pin
SWDIO ---> 7pin
SWCLK ---> 9pin

# UART standard
TODO write UART standard

# CAN standard
TODO write CAN standard
