#ifndef BOARD_CLOCK_CONFIG_H
#define BOARD_CLOCK_CONFIG_H

/* Общий источник частоты кварца для Board, HAL и CMSIS. Без зависимостей от HAL. */
#define BOARD_HSE_HZ 8000000U

#ifndef HSE_VALUE
#define HSE_VALUE BOARD_HSE_HZ
#elif HSE_VALUE != BOARD_HSE_HZ
#error "HSE_VALUE must match BOARD_HSE_HZ"
#endif

#endif /* BOARD_CLOCK_CONFIG_H */
