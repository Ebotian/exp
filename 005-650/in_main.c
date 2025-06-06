/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
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
#include <stdint.h>
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
// 信号采样缓冲
static uint8_t composite_sample = 0;

// 采样函数：每1ms采样一次COMPOSITE输入
static void Sample_Composite_Input(void) {
  composite_sample = HAL_GPIO_ReadPin(COMPOSITE_GPIO_Port, COMPOSITE_Pin);
}

// 解码状态机变量
static uint8_t composite_slot = 0;      // 0~3
static uint8_t tod_manchester[2] = {0}; // [0]=前半, [1]=后半
static uint8_t pps_manchester[2] = {0};

// 解码函数：每1ms调用一次，解码采样缓冲
static void Decode_Composite_Signal(void) {
  switch (composite_slot) {
  case 0: // TOD曼彻斯特前半
    tod_manchester[0] = composite_sample;
    break;
  case 1: // TOD曼彻斯特后半
    tod_manchester[1] = composite_sample;
    // 解码TOD
    if (tod_manchester[0] == GPIO_PIN_SET &&
        tod_manchester[1] == GPIO_PIN_RESET) {
      HAL_GPIO_WritePin(TOD_GPIO_Port, TOD_Pin, GPIO_PIN_SET); // 输出1
    } else if (tod_manchester[0] == GPIO_PIN_RESET &&
               tod_manchester[1] == GPIO_PIN_SET) {
      HAL_GPIO_WritePin(TOD_GPIO_Port, TOD_Pin, GPIO_PIN_RESET); // 输出0
    }
    break;
  case 2: // PPS曼彻斯特前半
    pps_manchester[0] = composite_sample;
    break;
  case 3: // PPS曼彻斯特后半
    pps_manchester[1] = composite_sample;
    // 解码PPS
    if (pps_manchester[0] == GPIO_PIN_SET &&
        pps_manchester[1] == GPIO_PIN_RESET) {
      HAL_GPIO_WritePin(PPS_GPIO_Port, PPS_Pin, GPIO_PIN_SET); // 输出1
    } else if (pps_manchester[0] == GPIO_PIN_RESET &&
               pps_manchester[1] == GPIO_PIN_SET) {
      HAL_GPIO_WritePin(PPS_GPIO_Port, PPS_Pin, GPIO_PIN_RESET); // 输出0
    }
    break;
  }
  composite_slot = (composite_slot + 1) % 4;
}
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick.
   */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint8_t composite_slot = 0;
  uint8_t tod_manchester[2] = {0};
  uint8_t pps_manchester[2] = {0};
  while (1) {
    // 1ms采样一次COMPOSITE输入
    uint8_t composite_sample =
        HAL_GPIO_ReadPin(COMPOSITE_GPIO_Port, COMPOSITE_Pin);
    switch (composite_slot) {
    case 0: // TOD曼彻斯特前半
      tod_manchester[0] = composite_sample;
      break;
    case 1: // TOD曼彻斯特后半
      tod_manchester[1] = composite_sample;
      // 解码TOD
      if (tod_manchester[0] == GPIO_PIN_SET &&
          tod_manchester[1] == GPIO_PIN_RESET) {
        HAL_GPIO_WritePin(TOD_GPIO_Port, TOD_Pin, GPIO_PIN_SET); // 输出1
      } else if (tod_manchester[0] == GPIO_PIN_RESET &&
                 tod_manchester[1] == GPIO_PIN_SET) {
        HAL_GPIO_WritePin(TOD_GPIO_Port, TOD_Pin, GPIO_PIN_RESET); // 输出0
      }
      break;
    case 2: // PPS曼彻斯特前半
      pps_manchester[0] = composite_sample;
      break;
    case 3: // PPS曼彻斯特后半
      pps_manchester[1] = composite_sample;
      // 解码PPS
      if (pps_manchester[0] == GPIO_PIN_SET &&
          pps_manchester[1] == GPIO_PIN_RESET) {
        HAL_GPIO_WritePin(PPS_GPIO_Port, PPS_Pin, GPIO_PIN_SET); // 输出1
      } else if (pps_manchester[0] == GPIO_PIN_RESET &&
                 pps_manchester[1] == GPIO_PIN_SET) {
        HAL_GPIO_WritePin(PPS_GPIO_Port, PPS_Pin, GPIO_PIN_RESET); // 输出0
      }
      break;
    }
    composite_slot = (composite_slot + 1) % 4;
    HAL_Delay(1); // 1ms周期
    /* USER CODE END WHILE */
    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
   */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) {
    Error_Handler();
  }
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, TOD_Pin | PPS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : COMPOSITE_Pin */
  GPIO_InitStruct.Pin = COMPOSITE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(COMPOSITE_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : TOD_Pin PPS_Pin */
  GPIO_InitStruct.Pin = TOD_Pin | PPS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1) {
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
void assert_failed(uint8_t *file, uint32_t line) {
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line
     number, ex: printf("Wrong parameters value: file %s on line %d\r\n", file,
     line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
