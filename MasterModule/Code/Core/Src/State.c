/**
 ******************************************************************************
 * @file    State.c
 * @brief   Определения глобального состояния приложения (см. State.h).
 ******************************************************************************
 */

#include "State.h"

/* ------------------------------- CAN ------------------------------------- */

uint32_t TxMailbox = 0;
uint32_t CAN_Error = 0;
HAL_StatusTypeDef HAL_Error;
fifo_t CANRxFIFO = NULL;

/* ------------------------------- UART ------------------------------------ */

UART_Message UARTRxTmpMessage = {};
UART_Message UARTTxTmpMessage = {};
fifo_t UARTRxFIFO = NULL;
fifo_t UARTTxFIFO = NULL;

/* ----------------------------- Приложение -------------------------------- */

int RegisteredByMonitor = 0;  // TODO bool

// TODO Rewrite this to Hash Table
struct PumpList Pumps = {};

int NeedToUartActions = 0;
int NeedToCANAction = 0;
