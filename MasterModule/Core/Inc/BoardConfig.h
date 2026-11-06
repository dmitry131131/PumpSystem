/**
 ******************************************************************************
 * @file    BoardConfig.h
 * @brief   Единственный источник истины по конфигурации платы MasterModule.
 *
 * Настройки периферии и фиксированная разводка MasterModule PCB.
 * Частота кварца общая для Board/HAL/CMSIS: см. BoardClockConfig.h.
 * Числовые значения *_NUM вычисляются из констант HAL, а не задаются отдельно.
 * Изменение разводки требует согласованных изменений Board.c и IRQ.c.
 ******************************************************************************
 */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "BoardClockConfig.h"
#include "stm32f1xx_hal.h"

/* ============================ Микроконтроллер ============================= */
/* STM32F103C8Tx, LQFP48, 64K FLASH / 20K RAM.
   Определяется макросом компиляции STM32F103xB (см. CMakeLists.txt). */

/* ============================== Тактирование ============================== */
/* Внешний кварц 8 МГц -> PLL x9 -> SYSCLK 72 МГц.
   AHB /2 -> HCLK 36 МГц, APB1 и APB2 без деления -> PCLK1 = PCLK2 = 36 МГц. */

#define BOARD_PLL_MUL             RCC_PLL_MUL9
#define BOARD_PLL_MUL_NUM         ((BOARD_PLL_MUL >> RCC_CFGR_PLLMULL_Pos) + 2U)
#define BOARD_SYSCLK_HZ           (BOARD_HSE_HZ * BOARD_PLL_MUL_NUM)  /* 72 МГц */

#define BOARD_AHB_PRESC           RCC_SYSCLK_DIV2
#define BOARD_APB1_PRESC          RCC_HCLK_DIV1
#define BOARD_APB2_PRESC          RCC_HCLK_DIV1

/* Декодирование допустимых делителей HAL; неизвестные значения отсекаются ниже. */
#define BOARD_AHB_DIV_NUM \
    (BOARD_AHB_PRESC == RCC_SYSCLK_DIV1 ? 1U : \
     BOARD_AHB_PRESC == RCC_SYSCLK_DIV2 ? 2U : \
     BOARD_AHB_PRESC == RCC_SYSCLK_DIV4 ? 4U : \
     BOARD_AHB_PRESC == RCC_SYSCLK_DIV8 ? 8U : \
     BOARD_AHB_PRESC == RCC_SYSCLK_DIV16 ? 16U : \
     BOARD_AHB_PRESC == RCC_SYSCLK_DIV64 ? 64U : \
     BOARD_AHB_PRESC == RCC_SYSCLK_DIV128 ? 128U : \
     BOARD_AHB_PRESC == RCC_SYSCLK_DIV256 ? 256U : 512U)
#define BOARD_APB_DIV_NUM(presc) \
    ((presc) == RCC_HCLK_DIV1 ? 1U : \
     (presc) == RCC_HCLK_DIV2 ? 2U : \
     (presc) == RCC_HCLK_DIV4 ? 4U : \
     (presc) == RCC_HCLK_DIV8 ? 8U : 16U)
#define BOARD_APB1_DIV_NUM        BOARD_APB_DIV_NUM(BOARD_APB1_PRESC)
#define BOARD_APB2_DIV_NUM        BOARD_APB_DIV_NUM(BOARD_APB2_PRESC)

#define BOARD_FLASH_LATENCY       FLASH_LATENCY_2

/* Рабочие частоты шин (выводятся из делителей выше) */
#define BOARD_HCLK_HZ             (BOARD_SYSCLK_HZ / BOARD_AHB_DIV_NUM)   /* 36 МГц */
#define BOARD_APB1_HZ             (BOARD_HCLK_HZ / BOARD_APB1_DIV_NUM)    /* 36 МГц */
#define BOARD_APB2_HZ             (BOARD_HCLK_HZ / BOARD_APB2_DIV_NUM)    /* 36 МГц */

/* ================================= CAN ==================================== */
/* bxCAN сидит на APB1. Тайминг: 36МГц / 45 = 800 кГц квант,
   800 кГц / (1 + 13 + 2) = 50 кбит/с.                                      */

#define BOARD_CAN_PRESC           45U
#define BOARD_CAN_BS1             CAN_BS1_13TQ
#define BOARD_CAN_BS2             CAN_BS2_2TQ
#define BOARD_CAN_SJW             CAN_SJW_1TQ
#define BOARD_CAN_BS1_NUM         ((BOARD_CAN_BS1 >> CAN_BTR_TS1_Pos) + 1U)
#define BOARD_CAN_BS2_NUM         ((BOARD_CAN_BS2 >> CAN_BTR_TS2_Pos) + 1U)
#define BOARD_CAN_SJW_NUM         ((BOARD_CAN_SJW >> CAN_BTR_SJW_Pos) + 1U)

#define BOARD_CAN_BAUD            50000U
#define BOARD_CAN_TQ_PER_BIT      (1U + BOARD_CAN_BS1_NUM + BOARD_CAN_BS2_NUM)  /* 16 */
#define BOARD_CAN_BITRATE_HZ      (BOARD_APB1_HZ / BOARD_CAN_PRESC / BOARD_CAN_TQ_PER_BIT)

/* Поведение bxCAN */
#define BOARD_CAN_MODE            CAN_MODE_NORMAL
#define BOARD_CAN_ABOM            ENABLE   /* автоматический выход из bus-off   */
#define BOARD_CAN_AUTO_RETRANSMISSION ENABLE /* автоматический повтор передачи */
#define BOARD_CAN_TXFP            ENABLE   /* приоритет по порядку в mailbox     */
#define BOARD_CAN_AUTO_WAKEUP     DISABLE
#define BOARD_CAN_RX_FIFO_LOCKED  DISABLE
#define BOARD_CAN_TTM             DISABLE  /* time triggered mode                */

/* Фильтр приёма: принимаем всё, решение по StdId принимает прикладной код
   (см. HAL_CAN_RxFifo0MsgPendingCallback -> сравнение с MY_ID). */
#define BOARD_CAN_FILTER_BANK     0U

/* ================================= UART =================================== */
/* USART1 на APB2 (36 МГц), 8N1, без аппаратного управления.                  */

#define BOARD_UART_BAUD           115200U
#define BOARD_UART_WORDLENGTH     UART_WORDLENGTH_8B
#define BOARD_UART_STOPBITS       UART_STOPBITS_1
#define BOARD_UART_PARITY         UART_PARITY_NONE
#define BOARD_UART_MODE           UART_MODE_TX_RX
#define BOARD_UART_FLOWCTL        UART_HWCONTROL_NONE
#define BOARD_UART_OVERSAMPLING   UART_OVERSAMPLING_16

/* =============================== Пины ===================================== */
/* Фиксированная разводка MasterModule PCB; проверки расположены ниже. */

/* PC13 — светодиод (индикация) */
#define BOARD_LED_PORT            GPIOC
#define BOARD_LED_PIN             GPIO_PIN_13

/* PB11 — EXTI, сигнал с шины (подтяжка вниз, оба фронта) */
#define BOARD_WAKEUP_PORT         GPIOB
#define BOARD_WAKEUP_PIN          GPIO_PIN_11
#define BOARD_WAKEUP_EXTI_MODE    GPIO_MODE_IT_RISING_FALLING
#define BOARD_WAKEUP_PULL         GPIO_PULLDOWN
#define BOARD_WAKEUP_EXTI_IRQ     EXTI15_10_IRQn
#define BOARD_WAKEUP_NVIC_PRIO    0U
#define BOARD_WAKEUP_NVIC_SUBPRIO 0U

/* CAN1 пере-map на PB8/PB9 (Default: PA11/PA12) */
#define BOARD_CAN_RX_PORT         GPIOB
#define BOARD_CAN_RX_PIN          GPIO_PIN_8
#define BOARD_CAN_TX_PORT         GPIOB
#define BOARD_CAN_TX_PIN          GPIO_PIN_9

/* USART1 — PA9 (TX) / PA10 (RX), дефолтное расположение */
#define BOARD_UART_TX_PORT        GPIOA
#define BOARD_UART_TX_PIN         GPIO_PIN_9
#define BOARD_UART_RX_PORT        GPIOA
#define BOARD_UART_RX_PIN         GPIO_PIN_10

/* Скорость/тип выходов */
#define BOARD_GPIO_SPEED_FAST     GPIO_SPEED_FREQ_HIGH  /* AF-выходы (CAN, UART) */
#define BOARD_GPIO_SPEED_LOW      GPIO_SPEED_FREQ_LOW   /* обычные выходы (LED)  */

/* ============================ Приоритеты NVIC ============================= */
/* Группировка приоритетов (NVIC_PRIORITYGROUP_4) выставляется внутри HAL_Init. */

#define BOARD_NVIC_PRIO_CAN_RX    1U   /* USB_LP_CAN1_RX0_IRQn */
#define BOARD_NVIC_PRIO_CAN_SCE   0U   /* CAN1_SCE_IRQn       */
#define BOARD_NVIC_PRIO_UART      0U   /* USART1_IRQn         */
#define BOARD_NVIC_PRIO_EXTI      BOARD_WAKEUP_NVIC_PRIO   /* EXTI15_10_IRQn      */

/* Приоритет SysTick берётся из TICK_INT_PRIORITY в stm32f1xx_hal_conf.h (=15). */

/* ============================== Отладка =================================== */
/* RESET: JTAG + SWD включены. JTAGDISABLE: только SWD, PA15/PB3/PB4 свободны.
   DISABLE: отключить оба интерфейса (для восстановления нужен reset). */
#define BOARD_SWJ_CONFIG          AFIO_MAPR_SWJ_CFG_RESET

/* ============================ Прикладной ID =============================== */
/* CAN StdId, которым адресуются сообщения Master-модуля. */
#define MY_ID                     100U

/* ======================== Статические проверки ============================
   Сообщения намеренно на ASCII: GCC экранирует не-ASCII байты в тексте
   ошибки (\37777777720...) и сообщение становится нечитаемым.               */

_Static_assert(IS_RCC_PLL_MUL(BOARD_PLL_MUL), "Invalid PLL multiplier");
_Static_assert(IS_RCC_HCLK(BOARD_AHB_PRESC), "Invalid AHB prescaler");
_Static_assert(IS_RCC_PCLK(BOARD_APB1_PRESC), "Invalid APB1 prescaler");
_Static_assert(IS_RCC_PCLK(BOARD_APB2_PRESC), "Invalid APB2 prescaler");
_Static_assert(BOARD_HSE_HZ >= 4000000U && BOARD_HSE_HZ <= 16000000U,
               "HSE crystal must be within 4..16 MHz");
_Static_assert(BOARD_SYSCLK_HZ >= 16000000U && BOARD_SYSCLK_HZ <= 72000000U,
               "PLL output must be within 16..72 MHz");
_Static_assert(BOARD_HCLK_HZ <= 72000000U, "HCLK exceeds 72 MHz");
_Static_assert(BOARD_APB1_HZ <= 36000000U, "APB1 exceeds 36 MHz");
_Static_assert(BOARD_APB2_HZ <= 72000000U, "APB2 exceeds 72 MHz");
_Static_assert(IS_FLASH_LATENCY(BOARD_FLASH_LATENCY), "Invalid Flash latency");
/* Для питания платы 3.3 V; лишние wait states допустимы. */
_Static_assert(BOARD_FLASH_LATENCY >= (BOARD_HCLK_HZ > 48000000U ? FLASH_LATENCY_2 :
                                     BOARD_HCLK_HZ > 24000000U ? FLASH_LATENCY_1 :
                                                               FLASH_LATENCY_0),
               "Flash latency too low for HCLK");
_Static_assert(IS_CAN_PRESCALER(BOARD_CAN_PRESC), "Invalid CAN prescaler");
_Static_assert(IS_CAN_BS1(BOARD_CAN_BS1), "Invalid CAN BS1");
_Static_assert(IS_CAN_BS2(BOARD_CAN_BS2), "Invalid CAN BS2");
_Static_assert(IS_CAN_SJW(BOARD_CAN_SJW), "Invalid CAN SJW");
_Static_assert(IS_CAN_MODE(BOARD_CAN_MODE), "Invalid CAN mode");
_Static_assert(BOARD_CAN_BAUD > 0U && BOARD_CAN_BAUD <= 1000000U,
               "CAN bitrate must be within 1..1000000 bit/s");
_Static_assert(IS_CAN_FILTER_BANK_SINGLE(BOARD_CAN_FILTER_BANK), "Invalid CAN filter bank");
_Static_assert(IS_CAN_STDID(MY_ID), "Master ID must fit an 11-bit CAN identifier");

_Static_assert((BOARD_SYSCLK_HZ % BOARD_AHB_DIV_NUM) == 0U,
               "Invalid AHB divider: SYSCLK not divisible");

_Static_assert((BOARD_HCLK_HZ % BOARD_APB1_DIV_NUM) == 0U,
               "Invalid APB1 divider: HCLK not divisible");

_Static_assert((BOARD_HCLK_HZ % BOARD_APB2_DIV_NUM) == 0U,
               "Invalid APB2 divider: HCLK not divisible");

_Static_assert((BOARD_APB1_HZ % (BOARD_CAN_PRESC * BOARD_CAN_TQ_PER_BIT)) == 0U,
               "CAN timing does not divide APB1 clock evenly");

_Static_assert(BOARD_CAN_BITRATE_HZ == BOARD_CAN_BAUD,
               "Computed CAN bitrate != BOARD_CAN_BAUD");

_Static_assert(BOARD_CAN_TQ_PER_BIT >= 8U && BOARD_CAN_TQ_PER_BIT <= 25U,
               "CAN time quanta per bit must be within 8..25");

_Static_assert(BOARD_CAN_SJW_NUM <= BOARD_CAN_BS2_NUM,
               "CAN SJW must not exceed BS2");

_Static_assert(BOARD_UART_BAUD > 0U && IS_UART_BAUDRATE(BOARD_UART_BAUD),
               "Invalid UART baud rate");
_Static_assert((UART_BRR_SAMPLING16(BOARD_APB2_HZ, BOARD_UART_BAUD)) >= 16U &&
               (UART_BRR_SAMPLING16(BOARD_APB2_HZ, BOARD_UART_BAUD)) <= 65535U,
               "UART BRR out of range for APB2 with oversampling 16");

/* Разводка и обработчики IRQ поддерживают именно эту плату. */
_Static_assert((uintptr_t)BOARD_LED_PORT == GPIOC_BASE && BOARD_LED_PIN == GPIO_PIN_13,
               "LED routing must be PC13");
_Static_assert((uintptr_t)BOARD_WAKEUP_PORT == GPIOB_BASE && BOARD_WAKEUP_PIN == GPIO_PIN_11 &&
               BOARD_WAKEUP_EXTI_IRQ == EXTI15_10_IRQn, "Wakeup routing must be PB11 / EXTI15_10");
_Static_assert((uintptr_t)BOARD_CAN_RX_PORT == GPIOB_BASE && BOARD_CAN_RX_PIN == GPIO_PIN_8 &&
               (uintptr_t)BOARD_CAN_TX_PORT == GPIOB_BASE && BOARD_CAN_TX_PIN == GPIO_PIN_9,
               "CAN1 routing must be PB8/PB9 with remap 2");
_Static_assert((uintptr_t)BOARD_UART_TX_PORT == GPIOA_BASE && BOARD_UART_TX_PIN == GPIO_PIN_9 &&
               (uintptr_t)BOARD_UART_RX_PORT == GPIOA_BASE && BOARD_UART_RX_PIN == GPIO_PIN_10,
               "USART1 routing must be PA9/PA10 without remap");
_Static_assert(BOARD_UART_WORDLENGTH == UART_WORDLENGTH_8B &&
               BOARD_UART_STOPBITS == UART_STOPBITS_1 && BOARD_UART_PARITY == UART_PARITY_NONE &&
               BOARD_UART_MODE == UART_MODE_TX_RX && BOARD_UART_FLOWCTL == UART_HWCONTROL_NONE &&
               BOARD_UART_OVERSAMPLING == UART_OVERSAMPLING_16,
               "UART transport requires 8N1 TX/RX without flow control, oversampling 16");
_Static_assert(BOARD_NVIC_PRIO_CAN_RX < 16U && BOARD_NVIC_PRIO_CAN_SCE < 16U &&
               BOARD_NVIC_PRIO_UART < 16U && BOARD_WAKEUP_NVIC_PRIO < 16U &&
               TICK_INT_PRIORITY < 16U && BOARD_WAKEUP_NVIC_SUBPRIO == 0U,
               "Invalid priority for NVIC_PRIORITYGROUP_4");
_Static_assert(BOARD_SWJ_CONFIG == AFIO_MAPR_SWJ_CFG_RESET ||
               BOARD_SWJ_CONFIG == AFIO_MAPR_SWJ_CFG_JTAGDISABLE ||
               BOARD_SWJ_CONFIG == AFIO_MAPR_SWJ_CFG_DISABLE, "Invalid SWJ configuration");

#endif /* BOARD_CONFIG_H */
