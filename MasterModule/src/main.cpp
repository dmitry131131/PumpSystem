#include "stm32f1xx_hal.h"

// Определение ножки для светодиода (PC13 на Blue Pill)
#define LED_PIN GPIO_PIN_13
#define LED_PORT GPIOC

// Прототипы функций
void SystemClock_Config(void);
void Error_Handler(void);

int main(void) {
    // Инициализация HAL
    HAL_Init();
    // Настройка системного такта
    SystemClock_Config();
    
    // Включаем тактирование порта C
    __HAL_RCC_GPIOC_CLK_ENABLE();
    
    // Настройка вывода для светодиода
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = LED_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_PORT, &GPIO_InitStruct);
    
    // Основной цикл
    while (1) {
        // Включаем светодиод (на Blue Pull активный уровень - LOW)
        HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);
        HAL_Delay(500);  // Задержка 500 мс
        
        // Выключаем светодиод
        HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);
        HAL_Delay(200);  // Задержка 500 мс
    }
}

// Конфигурация системного такта (используем внутренний RC генератор 8 МГц)
void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    
    // Настройка источника тактирования
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
    
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }
    
    // Настройка тактирования ядра и шин
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                  | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) {
        Error_Handler();
    }
}

void Error_Handler(void) {
    while (1) {
        // Мигаем светодиодом при ошибке (быстро)
        HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
        HAL_Delay(100);
    }
}

// Обработчик прерываний SysTick (нужен для HAL_Delay)
extern "C" void SysTick_Handler(void) {
    HAL_IncTick();
}