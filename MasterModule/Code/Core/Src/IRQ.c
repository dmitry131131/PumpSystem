/**
 ******************************************************************************
 * @file    IRQ.c
 * @brief   Обработчики прерываний и исключительных ситуаций Cortex-M3.
 *
 * Векторная таблица — в startup_stm32f103xb.s. Имена функций обязаны
 * совпадать с именами векторов.
 ******************************************************************************
 */

#include "Board.h"
#include "BoardConfig.h"

/* ====================== Исключения процессора ============================ */

void NMI_Handler(void)
{
  while (1) {
  }
}

void HardFault_Handler(void)
{
  while (1) {
  }
}

void MemManage_Handler(void)
{
  while (1) {
  }
}

void BusFault_Handler(void)
{
  while (1) {
  }
}

void UsageFault_Handler(void)
{
  while (1) {
  }
}

void SVC_Handler(void)
{
}

void DebugMon_Handler(void)
{
}

void PendSV_Handler(void)
{
}

/**
 * @brief  SysTick: отсчёт миллисекунд HAL.
 */
void SysTick_Handler(void)
{
  HAL_IncTick();
}

/* ====================== Периферийные прерывания =========================== */

/**
 * @brief  USB low priority / CAN RX0.
 */
void USB_LP_CAN1_RX0_IRQHandler(void)
{
  HAL_CAN_IRQHandler(&hcan);
}

/**
 * @brief  CAN status/change/error.
 */
void CAN1_SCE_IRQHandler(void)
{
  HAL_CAN_IRQHandler(&hcan);
}

/**
 * @brief  USART1 global interrupt.
 */
void USART1_IRQHandler(void)
{
  HAL_UART_IRQHandler(&huart1);
}

/**
 * @brief  EXTI lines 15:10 (PB11 — сигнал с шины).
 */
void EXTI15_10_IRQHandler(void)
{
  HAL_GPIO_EXTI_IRQHandler(BOARD_WAKEUP_PIN);
}
