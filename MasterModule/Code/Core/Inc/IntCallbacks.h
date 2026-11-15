/**
 ******************************************************************************
 * @file    IntCallbacks.h
 * @brief   Колбэки HAL, вызываемые из обработчиков прерываний.
 *
 * Реализация — в IntCallbacks.c. Перечислены явно, чтобы было видно,
 * какие слабые (weak) функции HAL переопределены приложением.
 ******************************************************************************
 */

#ifndef INT_CALLBACKS_H
#define INT_CALLBACKS_H

#include "stm32f1xx_hal.h"

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *can);
void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *can);
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart);

#endif /* INT_CALLBACKS_H */
