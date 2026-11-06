/**
 ******************************************************************************
 * @file    Board.c
 * @brief   Инициализация платы MasterModule.
 *
 * Содержит всё, что раньше было размазано по сгенерированным файлам:
 *   - SystemClock_Config() и MX_*_Init()        (бывший main.c)
 *   - HAL_MspInit / HAL_CAN_MspInit / HAL_UART_MspInit (бывший stm32f1xx_hal_msp.c)
 *   - Error_Handler()                           (бывший main.c)
 *
 * Все значения берутся из BoardConfig.h.
 ******************************************************************************
 */

#include "Board.h"
#include "BoardConfig.h"

/* ============================== Дескрипторы =============================== */

CAN_HandleTypeDef hcan;
UART_HandleTypeDef huart1;

/* ============================ Прототипы внутр. ============================ */

static void Board_ClockInit(void);
static void Board_GpioInit(void);
static void Board_CanInit(void);
static void Board_UartInit(void);

/* ================================ Внешний MSP ============================= */

/**
 * @brief  Глобальный MSP: тактирование AFIO/PWR и освобождение пинов отладки.
 */
void HAL_MspInit(void)
{
  __HAL_RCC_AFIO_CLK_ENABLE();
  __HAL_RCC_PWR_CLK_ENABLE();

  /* SWJ_CFG имеет неопределённое чтение: всегда записываем выбранный режим. */
  MODIFY_REG(AFIO->MAPR, AFIO_MAPR_SWJ_CFG, BOARD_SWJ_CONFIG);
}

/**
 * @brief  MSP для CAN1: тактирование, пины PB8/PB9, ремап, приоритеты NVIC.
 */
void HAL_CAN_MspInit(CAN_HandleTypeDef *can)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  if (can->Instance != CAN1) {
    return;
  }

  __HAL_RCC_CAN1_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /** CAN GPIO Configuration
    * PB8 ------> CAN_RX
    * PB9 ------> CAN_TX
    */
  GPIO_InitStruct.Pin = BOARD_CAN_RX_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BOARD_CAN_RX_PORT, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = BOARD_CAN_TX_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Speed = BOARD_GPIO_SPEED_FAST;
  HAL_GPIO_Init(BOARD_CAN_TX_PORT, &GPIO_InitStruct);

  /* bxCAN на PB8/PB9 вместо дефолтных PA11/PA12 */
  /* HAL remap-макрос этой версии затрагивает SWJ_CFG: задаём оба поля явно. */
  MODIFY_REG(AFIO->MAPR, AFIO_MAPR_CAN_REMAP | AFIO_MAPR_SWJ_CFG,
             AFIO_MAPR_CAN_REMAP_REMAP2 | BOARD_SWJ_CONFIG);

  HAL_NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, BOARD_NVIC_PRIO_CAN_RX, 0U);
  HAL_NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
  HAL_NVIC_SetPriority(CAN1_SCE_IRQn, BOARD_NVIC_PRIO_CAN_SCE, 0U);
  HAL_NVIC_EnableIRQ(CAN1_SCE_IRQn);
}

/**
 * @brief  De-Init MSP для CAN1.
 */
void HAL_CAN_MspDeInit(CAN_HandleTypeDef *can)
{
  if (can->Instance != CAN1) {
    return;
  }

  __HAL_RCC_CAN1_CLK_DISABLE();
  HAL_GPIO_DeInit(BOARD_CAN_RX_PORT, BOARD_CAN_RX_PIN);
  HAL_GPIO_DeInit(BOARD_CAN_TX_PORT, BOARD_CAN_TX_PIN);
  HAL_NVIC_DisableIRQ(USB_LP_CAN1_RX0_IRQn);
  HAL_NVIC_DisableIRQ(CAN1_SCE_IRQn);
}

/**
 * @brief  MSP для USART1: тактирование, пины PA9/PA10, приоритет NVIC.
 */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  if (huart->Instance != USART1) {
    return;
  }

  MODIFY_REG(AFIO->MAPR, AFIO_MAPR_USART1_REMAP | AFIO_MAPR_SWJ_CFG,
             BOARD_SWJ_CONFIG);
  __HAL_RCC_USART1_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /** USART1 GPIO Configuration
    * PA9 ------> USART1_TX
    * PA10 ------> USART1_RX
    */
  GPIO_InitStruct.Pin = BOARD_UART_TX_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Speed = BOARD_GPIO_SPEED_FAST;
  HAL_GPIO_Init(BOARD_UART_TX_PORT, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = BOARD_UART_RX_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BOARD_UART_RX_PORT, &GPIO_InitStruct);

  HAL_NVIC_SetPriority(USART1_IRQn, BOARD_NVIC_PRIO_UART, 0U);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
}

/**
 * @brief  De-Init MSP для USART1.
 */
void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
  if (huart->Instance != USART1) {
    return;
  }

  __HAL_RCC_USART1_CLK_DISABLE();
  HAL_GPIO_DeInit(BOARD_UART_TX_PORT, BOARD_UART_TX_PIN);
  HAL_GPIO_DeInit(BOARD_UART_RX_PORT, BOARD_UART_RX_PIN);
  HAL_NVIC_DisableIRQ(USART1_IRQn);
}

/* ============================== Инициализация ============================= */

void Board_Init(void)
{
  HAL_Init();
  Board_ClockInit();
  Board_GpioInit();
  Board_CanInit();
  Board_UartInit();
}

/**
 * @brief  Тактирование: HSE 8 МГц -> PLL x9 -> SYSCLK 72 МГц,
 *         HCLK 36 МГц (AHB/2), PCLK1 = PCLK2 = 36 МГц.
 */
static void Board_ClockInit(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /* Осцилляторы: внешний кварц + PLL от HSE.
     HSI не входит в OscillatorType — после сброса он и так включён,
     и HAL его не трогает (ровно как в исходном коде). */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = BOARD_PLL_MUL;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }

  /* Шины: SYSCLK = PLL, AHB/2, APB1/1, APB2/1 */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                              | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = BOARD_AHB_PRESC;
  RCC_ClkInitStruct.APB1CLKDivider = BOARD_APB1_PRESC;
  RCC_ClkInitStruct.APB2CLKDivider = BOARD_APB2_PRESC;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, BOARD_FLASH_LATENCY) != HAL_OK) {
    Error_Handler();
  }
}

/**
 * @brief  GPIO: светодиод PC13 и EXTI-вход PB11 (сигнал с шины).
 *
 *  Остальные пины (CAN PB8/PB9, UART PA9/PA10) настраиваются
 *  в своих MSP-хуках при инициализации периферий.
 */
static void Board_GpioInit(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* Порты фиксированной разводки MasterModule PCB. */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /* Светодиод: выход, старт в выключенном состоянии */
  HAL_GPIO_WritePin(BOARD_LED_PORT, BOARD_LED_PIN, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = BOARD_LED_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = BOARD_GPIO_SPEED_LOW;
  HAL_GPIO_Init(BOARD_LED_PORT, &GPIO_InitStruct);

  /* EXTI: сигнал с шины, оба фронта, подтяжка вниз */
  GPIO_InitStruct.Pin = BOARD_WAKEUP_PIN;
  GPIO_InitStruct.Mode = BOARD_WAKEUP_EXTI_MODE;
  GPIO_InitStruct.Pull = BOARD_WAKEUP_PULL;
  HAL_GPIO_Init(BOARD_WAKEUP_PORT, &GPIO_InitStruct);

  HAL_NVIC_SetPriority(BOARD_WAKEUP_EXTI_IRQ,
                       BOARD_WAKEUP_NVIC_PRIO, BOARD_WAKEUP_NVIC_SUBPRIO);
  HAL_NVIC_EnableIRQ(BOARD_WAKEUP_EXTI_IRQ);
}

/**
 * @brief  CAN1: 50 кбит/с (см. тайминг в BoardConfig.h).
 */
static void Board_CanInit(void)
{
  hcan.Instance = CAN1;
  hcan.Init.Prescaler = BOARD_CAN_PRESC;
  hcan.Init.Mode = BOARD_CAN_MODE;
  hcan.Init.SyncJumpWidth = BOARD_CAN_SJW;
  hcan.Init.TimeSeg1 = BOARD_CAN_BS1;
  hcan.Init.TimeSeg2 = BOARD_CAN_BS2;
  hcan.Init.TimeTriggeredMode = BOARD_CAN_TTM;
  hcan.Init.AutoBusOff = BOARD_CAN_ABOM;
  hcan.Init.AutoWakeUp = BOARD_CAN_AUTO_WAKEUP;
  hcan.Init.AutoRetransmission = BOARD_CAN_AUTO_RETRANSMISSION;
  hcan.Init.ReceiveFifoLocked = BOARD_CAN_RX_FIFO_LOCKED;
  hcan.Init.TransmitFifoPriority = BOARD_CAN_TXFP;
  if (HAL_CAN_Init(&hcan) != HAL_OK) {
    Error_Handler();
  }
}

/**
 * @brief  USART1: 115200 8N1 (см. BoardConfig.h).
 */
static void Board_UartInit(void)
{
  huart1.Instance = USART1;
  huart1.Init.BaudRate = BOARD_UART_BAUD;
  huart1.Init.WordLength = BOARD_UART_WORDLENGTH;
  huart1.Init.StopBits = BOARD_UART_STOPBITS;
  huart1.Init.Parity = BOARD_UART_PARITY;
  huart1.Init.Mode = BOARD_UART_MODE;
  huart1.Init.HwFlowCtl = BOARD_UART_FLOWCTL;
  huart1.Init.OverSampling = BOARD_UART_OVERSAMPLING;
  if (HAL_UART_Init(&huart1) != HAL_OK) {
    Error_Handler();
  }
}

/* =============================== Error_Handler ============================ */

void Error_Handler(void)
{
  __disable_irq();
  while (1) {
  }
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Отчёт о неверном параметре assert_failed().
 * @param  file: имя файла
 * @param  line: номер строки
 */
void assert_failed(uint8_t *file, uint32_t line)
{
  (void)file;
  (void)line;
}
#endif /* USE_FULL_ASSERT */
