#pragma once
#include "stm32f1xx_hal.h"

#include "UART.h"
#include "FIFO.h"

void CANRuntime(CAN_HandleTypeDef *hcan, fifo_t CANRxFIFO);