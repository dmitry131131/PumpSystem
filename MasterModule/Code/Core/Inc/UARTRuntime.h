#pragma once
#include "stm32f1xx_hal.h"

#include "UART.h"
#include "FIFO.h"

void UARTRuntime(UART_HandleTypeDef *huart, fifo_t CANTxFIFO, fifo_t UARTRxFIFO, fifo_t UARTTxFIFO);
