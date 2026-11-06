/**
 ******************************************************************************
 * @file    Board.h
 * @brief   Инициализация платы MasterModule: тактирование, GPIO, CAN, UART.
 *
 * Владеет дескрипторами периферий (hcan, huart1). Вся параметризация —
 * в BoardConfig.h.
 ******************************************************************************
 */

#ifndef BOARD_H
#define BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

/* Дескрипторы периферий (определены в Board.c) */
extern CAN_HandleTypeDef hcan;
extern UART_HandleTypeDef huart1;

/**
 * @brief  Полная инициализация платы.
 *         HAL_Init + тактирование + GPIO + CAN + UART.
 *         Вызывается один раз из main() до всей прикладной инициализации.
 */
void Board_Init(void);

/**
 * @brief  Аварийная остановка: IRQ отключены, бесконечное ожидание.
 */
void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_H */
