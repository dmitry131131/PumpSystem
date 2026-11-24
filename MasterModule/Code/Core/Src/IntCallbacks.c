/**
 ******************************************************************************
 * @file    IntCallbacks.c
 * @brief   Прикладные колбэки HAL, вызываемые из обработчиков прерываний.
 *
 * Сюда перенесён весь блок «USER CODE 0» из бывшего main.c.
 ******************************************************************************
 */

#include "Board.h"
#include "BoardConfig.h"
#include "BusConnection.h"
#include "IntCallbacks.h"
#include "State.h"

// A complete 11-byte frame at 115200 baud takes about 1 ms on the wire.
static const uint32_t uartRxGapMs = 25U;
static UART_Message uartRxMessage;
static uint8_t uartRxByte;
static size_t uartRxSize;
static uint32_t uartRxLastTick;
static bool uartRxDropUntilGap;

HAL_StatusTypeDef StartMonitorUartRx(UART_HandleTypeDef *huart)
{
  return HAL_UART_Receive_IT(huart, &uartRxByte, 1U);
}

/**
 * @brief  Пришло сообщение в CAN FIFO0.
 *         Отфильтровываем по своему ID и кладём в CANRxFIFO.
 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *can)
{
  CANRxMessage NewMessage = {};

  if (HAL_CAN_GetRxMessage(can, CAN_RX_FIFO0, &NewMessage.Header,
                           NewMessage.RxData) != HAL_OK) {
    return;
  }
  if (NewMessage.Header.IDE != CAN_ID_STD || NewMessage.Header.RTR != CAN_RTR_DATA ||
      NewMessage.Header.StdId != MY_ID) {
    return;
  }
  if (NewMessage.Header.DLC < 1) {
    return;
  }

  // TODO check return value of fifo add
  fifo_add(CANRxFIFO, &NewMessage);
}

/**
 * @brief  Ошибка шины CAN (error passive, bus-off и т.п.).
 */
void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *can)
{
  CAN_Error = HAL_CAN_GetError(&hcan);

  // HAL_GPIO_WritePin(BOARD_LED_PORT, BOARD_LED_PIN, GPIO_PIN_RESET);
}

/**
 * @brief  EXTI: сработал вход PB11 (сигнал с шины).
 *         Мигаем светодиодом и просим разобраться с UART-очередью.
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  HAL_GPIO_TogglePin(BOARD_LED_PORT, BOARD_LED_PIN);

  if (GPIO_Pin != BOARD_WAKEUP_PIN) {
    return;
  }

  NeedToUartActions = 1;
}

/**
 * @brief  Accumulate UART bytes into a frame and discard incomplete bursts after a gap.
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  // HAL can call this before its error callback. Leave recovery to that callback.
  if (huart != &huart1 || HAL_UART_GetError(huart) != HAL_UART_ERROR_NONE) {
    return;
  }

  const uint32_t now = HAL_GetTick();
  if (now - uartRxLastTick >= uartRxGapMs) {
    uartRxSize = 0;
    uartRxDropUntilGap = false;
  }
  uartRxLastTick = now;

  if (!uartRxDropUntilGap) {
    ((uint8_t *)&uartRxMessage)[uartRxSize++] = uartRxByte;
    if (uartRxSize == sizeof(uartRxMessage)) {
      fifo_add(UARTRxFIFO, &uartRxMessage);
      uartRxSize = 0;
    }
  }

  StartMonitorUartRx(huart);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart != &huart1) {
    return;
  }

  uartRxSize = 0;
  uartRxLastTick = HAL_GetTick();
  uartRxDropUntilGap = true;
  HAL_UART_AbortReceive(huart);
  __HAL_UART_CLEAR_OREFLAG(huart); // Read SR and DR to clear STM32F1 RX errors.
  StartMonitorUartRx(huart);
}

/**
 * @brief  Передача по UART завершена.
 *         Отправка ведётся вручную из UARTRuntime(), здесь ничего не нужно.
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  // if (!fifo_is_empty(UARTTxFIFO)) {
  //   // Pop already sent message from the TxFIFO
  //   fifo_discard(UARTTxFIFO, 1, E_FIFO_FRONT);
  // }
  // if (fifo_is_empty(UARTTxFIFO)) {
  //   return;
  // }

  // fifo_get(UARTTxFIFO, &UARTTxTmpMessage);
  // HAL_UART_Transmit_IT(&huart1, (uint8_t*) &UARTTxTmpMessage, sizeof(UART_Message));
}
