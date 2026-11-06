/**
 ******************************************************************************
 * @file    main.c
 * @brief   Точка входа MasterModule.
 *
 * Здесь только прикладная логика: создание очередей, запуск CAN и главный
 * цикл. Инициализация аппаратуры — Board_Init(), колбэки — IntCallbacks.c,
 * обработчики прерываний — IRQ.c.
 ******************************************************************************
 */

#include "Board.h"
#include "BoardConfig.h"

#include "BusConnection.h"
#include "BusConnectionConfig.h"
#include "CANRuntime.h"
#include "Device.h"
#include "FIFO.h"
#include "State.h"
#include "UART.h"
#include "UARTRuntime.h"

/**
 * @brief  Application entry point.
 * @retval int
 */
int main(void)
{
  /* Инициализация платы: HAL + тактирование + GPIO + CAN + UART */
  Board_Init();

  /* ---------------------- Прикладная инициализация ----------------------- */

  PumpListInit(&Pumps, 16);

  CANRxFIFO = fifo_create(64, sizeof(CANRxMessage));

  // Create UART FIFOs
  UARTRxFIFO = fifo_create(64, sizeof(UART_Message));
  UARTTxFIFO = fifo_create(64, sizeof(UART_Message));

  /* Приём всех сообщений; отбор по MY_ID делается в колбэке. */
  CAN_FilterTypeDef sFilterConfig;
  sFilterConfig.FilterBank = BOARD_CAN_FILTER_BANK;
  sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
  sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
  sFilterConfig.FilterIdLow = 0;
  sFilterConfig.FilterIdHigh = 0;
  sFilterConfig.FilterMaskIdLow = 0;
  sFilterConfig.FilterMaskIdHigh = 0;
  sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
  sFilterConfig.FilterActivation = ENABLE;

  if (HAL_CAN_ConfigFilter(&hcan, &sFilterConfig) != HAL_OK) {
    Error_Handler();
  }

  HAL_Error = HAL_CAN_Start(&hcan);
  HAL_Error = HAL_CAN_ActivateNotification(&hcan,
      CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_ERROR | CAN_IT_BUSOFF | CAN_IT_LAST_ERROR_CODE);

  /* ------------------------------------------------------------------------ */

  HAL_GPIO_WritePin(BOARD_LED_PORT, BOARD_LED_PIN, GPIO_PIN_SET);

  // Start receiving UART messages
  HAL_UART_Receive_IT(&huart1, (uint8_t *)&UARTRxTmpMessage, sizeof(UART_Message));

  while (1) {
    // Indicated Registration
    if (RegisteredByMonitor) {
      HAL_GPIO_WritePin(BOARD_LED_PORT, BOARD_LED_PIN, GPIO_PIN_RESET);
    }

    // UART runtime
    UARTRuntime(&huart1, &hcan, UARTRxFIFO, UARTTxFIFO);

    // TODO Create CAN FIFO
    // TODO Handle CAN error and CAN bus-off situations
    // CAN runtime
    CANRuntime(&hcan, CANRxFIFO);
  }

  PumpListFree(&Pumps);
}
