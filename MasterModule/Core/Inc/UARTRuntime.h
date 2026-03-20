#pragma once
#include "stm32f1xx_hal.h"

#include "UART.h"
#include "FIFO.h"

void UARTRuntime(UART_HandleTypeDef *huart, fifo_t RxFIFO, fifo_t TxFIFO);