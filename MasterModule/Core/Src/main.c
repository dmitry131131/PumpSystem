/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "Device.h"
#include "BusConnectionConfig.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
const uint32_t MY_ID = 100;
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CAN_HandleTypeDef hcan;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

uint32_t TxMailbox = 0;
uint32_t CAN_Error = 0;
HAL_StatusTypeDef HAL_Error;

struct PumpList Pumps = {};

int NeedToUartActions = 0;
int NeedToCANAction = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_CAN_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

CAN_TxHeaderTypeDef CreateRegistrationResponseHeader(uint32_t DeviceId) {
  CAN_TxHeaderTypeDef TxHeader;

  TxHeader.StdId = DeviceId;
  TxHeader.ExtId = 0;
  TxHeader.RTR = CAN_RTR_DATA;
  TxHeader.IDE = CAN_ID_STD;
  TxHeader.DLC = 1;
  TxHeader.TransmitGlobalTime = 0;

  return TxHeader;
}

CAN_TxHeaderTypeDef CreateStartCommandHeader() {
  CAN_TxHeaderTypeDef TxHeader;

  TxHeader.StdId = COMMAND_START;
  TxHeader.ExtId = 0;
  TxHeader.RTR = CAN_RTR_DATA;
  TxHeader.IDE = CAN_ID_STD;
  TxHeader.DLC = 0;
  TxHeader.TransmitGlobalTime = 0;

  return TxHeader;
}

CAN_TxHeaderTypeDef CreateRotationOperationHeader(uint32_t DeviceId) {
  CAN_TxHeaderTypeDef TxHeader;

  TxHeader.StdId = DeviceId;
  TxHeader.ExtId = 0;
  TxHeader.RTR = CAN_RTR_DATA;
  TxHeader.IDE = CAN_ID_STD;
  TxHeader.DLC = 8;
  TxHeader.TransmitGlobalTime = 0;

  return TxHeader;
}

void CreateRotationOperationData(uint8_t* TxData, enum RotationDirection direction, float degree, uint8_t time) {
  union {
    float f;
    uint8_t bytes[4];
  } converter;

  TxData[0] = DATA_PACKAGE;
  TxData[1] = ROTATION;
  TxData[2] = direction;

  converter.f = degree;
  for (size_t i = 0; i < sizeof(converter.bytes) / sizeof(uint8_t); ++i) {
    TxData[3 + i] = converter.bytes[i];
  }

  TxData[7] = time;
}

CAN_TxHeaderTypeDef CreateClearDataBufferHeader(uint32_t DeviceId) {
  CAN_TxHeaderTypeDef TxHeader;

  TxHeader.StdId = DeviceId;
  TxHeader.ExtId = 0;
  TxHeader.RTR = CAN_RTR_DATA;
  TxHeader.IDE = CAN_ID_STD;
  TxHeader.DLC = 1;
  TxHeader.TransmitGlobalTime = 0;

  return TxHeader;
}

void CreateClearDataBufferData(uint8_t* TxData) {
  TxData[0] = CLEAR_DATA_BUFFER;
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *can) {
  CAN_RxHeaderTypeDef RxHeader;
  uint8_t RxData[8] = {};
  if (HAL_CAN_GetRxMessage(can, CAN_RX_FIFO0, &RxHeader, RxData) != HAL_OK) {
    return;
  }

  if (RxHeader.StdId != MY_ID) {
    return;
  }

  if (RxHeader.DLC < 1) {
    return;
  }

  switch (RxData[0])
  {
  // Registration message
  case REGISTRATION_REQUEST:
    {
      if (RxHeader.DLC != 2) {
        return;
      }

      struct Pump *Old = FindPump(&Pumps, RxData[1]);
      // If Pump already in list 
      if (Old) {
        Old->State = PUMP_ENABLE;
        CAN_TxHeaderTypeDef TxHeader = CreateRegistrationResponseHeader(Old->Id);
        uint8_t TxData[8] = {};
        TxData[0] = REGISTRATION_SUCCESS; // Registration response code
        HAL_Error = HAL_CAN_AddTxMessage(&hcan, &TxHeader, TxData, &TxMailbox);

        return;
      }

      struct Pump New = {.Id = RxData[1],
                          .State = PUMP_ENABLE};  // Get Pump ID

      // Create registration response header
      CAN_TxHeaderTypeDef TxHeader = CreateRegistrationResponseHeader(New.Id);
      uint8_t TxData[8] = {};
      if (AddPump(&Pumps, New)) {
      // Fill the CAN TxData
      TxData[0] = REGISTRATION_SUCCESS; // Registration response code
      }
      else {
        TxData[0] = REGISTRATION_DECLINED; // Registration decline code
      }

      HAL_Error = HAL_CAN_AddTxMessage(&hcan, &TxHeader, TxData, &TxMailbox);
    }
    break;
  // In case changing lock status it is need to clear pump OperationBuffer
  case FORWARD_LOCK_REACHED: 
    {
      if (RxHeader.DLC != 1) {
        return;
      }
      // If not registered
      struct Pump *Pump = FindPump(&Pumps, RxData[1]); 
      if (!Pump) {
        return;
      }

      Pump->State = PUMP_BLOCKED_FORWARD;

      // Send clear data buffer command
      CAN_TxHeaderTypeDef TxHeader = CreateClearDataBufferHeader(RxData[1]);
      uint8_t TxData[8] = {};
      CreateClearDataBufferData(TxData);
      HAL_Error = HAL_CAN_AddTxMessage(&hcan, &TxHeader, TxData, &TxMailbox);
    }
    break;
  case REVERSE_LOCK_REACHED: 
    {
      if (RxHeader.DLC != 1) {
        return;
      }
      // If not registered
      struct Pump *Pump = FindPump(&Pumps, RxData[1]); 
      if (!Pump) {
        return;
      }

      Pump->State = PUMP_BLOCKED_REVERSE;

      // Send clear data buffer command
      CAN_TxHeaderTypeDef TxHeader = CreateClearDataBufferHeader(RxData[1]);
      uint8_t TxData[8] = {};
      CreateClearDataBufferData(TxData);
      HAL_Error = HAL_CAN_AddTxMessage(&hcan, &TxHeader, TxData, &TxMailbox);
    }
    break;
  case REVERSE_LOCK_RELEASED:
  case FORWARD_LOCK_RELEASED: 
    {
      if (RxHeader.DLC != 1) {
        return;
      }
      // If not registered
      struct Pump *Pump = FindPump(&Pumps, RxData[1]); 
      if (!Pump) {
        return;
      }

      Pump->State = PUMP_ENABLE;

      // Send clear data buffer command
      CAN_TxHeaderTypeDef TxHeader = CreateClearDataBufferHeader(RxData[1]);
      uint8_t TxData[8] = {};
      CreateClearDataBufferData(TxData);
      HAL_Error = HAL_CAN_AddTxMessage(&hcan, &TxHeader, TxData, &TxMailbox);
    }
    break;
  
  default:
    break;
  }
}

void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *can) {
  CAN_Error = HAL_CAN_GetError(&hcan);
  // HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
  
  if(GPIO_Pin != GPIO_PIN_11) {
    return;
  }

  NeedToUartActions = 1;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_CAN_Init();
  // MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  PumpListInit(&Pumps, 16);

  CAN_FilterTypeDef sFilterConfig;
  sFilterConfig.FilterBank = 0;
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
  HAL_Error = HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_ERROR | CAN_IT_BUSOFF | CAN_IT_LAST_ERROR_CODE);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
  uint8_t Hello[] = "Hello\n";

  while (Pumps.size == 0)
  {
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
    HAL_Delay(70);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    HAL_Delay(1500);
    // HAL_UART_Transmit(&huart1, Hello, sizeof(Hello), 100);
  }

  // CAN_TxHeaderTypeDef OperationHeader = CreateRotationOperationHeader(Pumps.List[0].Id);
  // uint8_t OperationData[8] = {};
  // CreateRotationOperationData(OperationData, FORWARD, 90, 6);
  // HAL_Error = HAL_CAN_AddTxMessage(&hcan, &OperationHeader, OperationData, &TxMailbox);
  // CreateRotationOperationData(OperationData, REVERSE, 90, 6);
  // HAL_Error = HAL_CAN_AddTxMessage(&hcan, &OperationHeader, OperationData, &TxMailbox);
  // HAL_Delay(500);

  int UART_INIT = 0;

  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    if (NeedToUartActions) {
      NeedToUartActions = 0;

      if (!UART_INIT) {
        MX_USART1_UART_Init();
        UART_INIT = 1;
      } else {
        HAL_UART_DeInit(&huart1);
        UART_INIT = 0;
      }

      // HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
      // HAL_Delay(1500);
      // HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
      // HAL_Delay(1500);
    }




  //   CAN_TxHeaderTypeDef StartCommand = CreateStartCommandHeader();
  //   uint8_t TxData[8] = {};
  //   HAL_Error = HAL_CAN_AddTxMessage(&hcan, &StartCommand, TxData, &TxMailbox);

  //   HAL_Delay(6000);

  // CAN_TxHeaderTypeDef GarbageHeader = CreateRotationOperationHeader(102);
  // uint8_t GarbageData[8] = {};
  // CreateRotationOperationData(GarbageData, FORWARD, 90, 6);
  // HAL_Error = HAL_CAN_AddTxMessage(&hcan, &GarbageHeader, GarbageData, &TxMailbox);
  // HAL_Delay(2000);
  }

  PumpListFree(&Pumps);
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief CAN Initialization Function
  * @param None
  * @retval None
  */
static void MX_CAN_Init(void)
{

  /* USER CODE BEGIN CAN_Init 0 */

  /* USER CODE END CAN_Init 0 */

  /* USER CODE BEGIN CAN_Init 1 */

  /* USER CODE END CAN_Init 1 */
  hcan.Instance = CAN1;
  hcan.Init.Prescaler = 45;
  hcan.Init.Mode = CAN_MODE_NORMAL;
  hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan.Init.TimeSeg1 = CAN_BS1_13TQ;
  hcan.Init.TimeSeg2 = CAN_BS2_2TQ;
  hcan.Init.TimeTriggeredMode = DISABLE;
  hcan.Init.AutoBusOff = ENABLE;
  hcan.Init.AutoWakeUp = DISABLE;
  hcan.Init.AutoRetransmission = ENABLE;
  hcan.Init.ReceiveFifoLocked = DISABLE;
  hcan.Init.TransmitFifoPriority = ENABLE;
  if (HAL_CAN_Init(&hcan) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN_Init 2 */

  /* USER CODE END CAN_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PB11 */
  GPIO_InitStruct.Pin = GPIO_PIN_11;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
