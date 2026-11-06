/**
 ******************************************************************************
 * @file    State.h
 * @brief   Глобальное состояние приложения MasterModule.
 *
 * Раньше эти переменные были определены в main.c и разъезжались по файлам
 * через локальные extern-объявления. Теперь все объявления собраны здесь.
 ******************************************************************************
 */

#ifndef STATE_H
#define STATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

#include "Device.h"
#include "FIFO.h"
#include "UART.h"

/* ------------------------------- CAN ------------------------------------- */

/* Mailbox, в который кладутся исходящие CAN-сообщения */
extern uint32_t TxMailbox;

/* Последняя ошибка CAN (см. HAL_CAN_ErrorCallback) */
extern uint32_t CAN_Error;

/* Статус последней операции HAL (сохраняется для отладки) */
extern HAL_StatusTypeDef HAL_Error;

/* Очередь принятых CAN-сообщений */
extern fifo_t CANRxFIFO;

/* ------------------------------- UART ------------------------------------ */

/* Буфер приёма/передачи прерываниями. Заполняется в колбэках,
   читается/отправляется из main-цикла. */
extern UART_Message UARTRxTmpMessage;
extern UART_Message UARTTxTmpMessage;

extern fifo_t UARTRxFIFO;
extern fifo_t UARTTxFIFO;

/* ----------------------------- Приложение -------------------------------- */

/* 1 — мастер зарегистрирован монитором (по UART) */
extern int RegisteredByMonitor;

/* Реестр подключённых насосов */
extern struct PumpList Pumps;

/* Флаги «что нужно сделать», выставляются из прерываний */
extern int NeedToUartActions;
extern int NeedToCANAction;

#ifdef __cplusplus
}
#endif

#endif /* STATE_H */
