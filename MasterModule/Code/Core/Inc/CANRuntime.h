#pragma once
#include "stm32f1xx_hal.h"

#include "UART.h"
#include "FIFO.h"

void CANRuntime(fifo_t CANRxFIFO, fifo_t CANTxFIFO);
void CANAttendanceRuntime(fifo_t CANTxFIFO);
void CANTxRuntime(CAN_HandleTypeDef *hcan, fifo_t CANTxFIFO);
