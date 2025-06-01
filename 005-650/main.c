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
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;

/* USER CODE BEGIN PV */
#define PREAMBLE_MS 150 // 前导码时长 (ms)
#define TOD_BYTE_COUNT 20
static uint8_t in_preamble = 1; // 当前是否在前导码阶段
static uint16_t preamble_ms_count = 0;  // 前导码时长计数 (ms)
static uint16_t preamble_half_tick = 0; // 半比特切换计数
// Manchester 编码参数定义
#define MANCHESTER_HALF_MS 5                                  // 半比特时长 (ms)
static const uint8_t bit_stream[] = {1, 0, 1, 1, 0, 0, 1, 0}; // 示例比特流
static const uint16_t stream_len = sizeof(bit_stream) / sizeof(bit_stream[0]);
static uint16_t bit_index = 0;
static uint16_t man_tick = 0;
static uint8_t man_half = 0; // 0=前半周期,1=后半周期
#define TOD_BIT_MS 1         // 仿真每位时长 (ms)
// 新增：原始 TOD 串口仿真缓冲 & 状态
static const uint8_t tod_frame[TOD_BYTE_COUNT] = {
    0x43, 0x4D, 0x01, 0x20, 0x00, 0x10, 0x00, 0x02, 0xFF, 0x45,
    0x00, 0x00, 0x00, 0x00, 0x06, 0x16, 0x0F, 0x00, 0xFF, 0x17};
static uint8_t ser_bits[TOD_BYTE_COUNT * 10];
static uint16_t ser_len = 0, ser_idx = 0;
static uint16_t ms_since_pps = 0;
static uint8_t prev_pps = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
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

  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  HAL_GPIO_TogglePin(DEBUG_LED_GPIO_Port, DEBUG_LED_Pin);
  for (volatile uint32_t d = 0; d < 1000000; d++)
    ; // 大约 50ms～100ms
  // 1PPS PWM
  MX_TIM3_Init();
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
  HAL_GPIO_TogglePin(DEBUG_LED_GPIO_Port, DEBUG_LED_Pin);
  for (volatile uint32_t d = 0; d < 1000000; d++)
    ; // 大约 50ms～100ms
  // TOD 中断
  MX_TIM2_Init();
  //---- 新增 NVIC 使能 TIM2 ----
  HAL_NVIC_SetPriority(TIM2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(TIM2_IRQn);
  //---- 结束 ----
  HAL_TIM_Base_Start_IT(&htim2);
  HAL_GPIO_TogglePin(DEBUG_LED_GPIO_Port, DEBUG_LED_Pin);
  for (volatile uint32_t d = 0; d < 1000000; d++)
    ; // 大约 50ms～100ms
  while (1) {
    // 调试灯翻转
    HAL_GPIO_TogglePin(DEBUG_LED_GPIO_Port, DEBUG_LED_Pin);
    HAL_Delay(200);
  }
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
 * @brief TIM2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM2_Init(void) {

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 15;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK) {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK) {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
}

/**
 * @brief TIM3 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM3_Init(void) {

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 1599;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 9999;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK) {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK) {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK) {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK) {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 1000;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_2) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */
  HAL_TIM_MspPostInit(&htim3);
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
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE(); // 确保COMPOSITE端口时钟已启用

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(TOD_GPIO_Port, TOD_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(DEBUG_LED_GPIO_Port, DEBUG_LED_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(COMPOSITE_GPIO_Port, COMPOSITE_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : TOD_Pin */
  GPIO_InitStruct.Pin = TOD_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(TOD_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : DEBUG_LED_Pin */
  GPIO_InitStruct.Pin = DEBUG_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(DEBUG_LED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : COMPOSITE_Pin */
  GPIO_InitStruct.Pin = COMPOSITE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(COMPOSITE_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
// Manchester 编码输出函数
static void Manchester_Update(void) {
  if (++man_tick >= MANCHESTER_HALF_MS) {
    man_tick = 0;
    if (man_half == 0) {
      HAL_GPIO_WritePin(COMPOSITE_GPIO_Port, COMPOSITE_Pin,
                        bit_stream[bit_index] ? GPIO_PIN_SET : GPIO_PIN_RESET);
      man_half = 1;
    } else {
      HAL_GPIO_WritePin(COMPOSITE_GPIO_Port, COMPOSITE_Pin,
                        bit_stream[bit_index] ? GPIO_PIN_RESET : GPIO_PIN_SET);
      man_half = 0;
      if (++bit_index >= stream_len)
        bit_index = 0;
    }
  }
}

// TIM2每1ms中断：优先输出前导码50%方波，完成后切入Manchester编码，PPS由TIM3
// PWM输出
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  if (htim->Instance == TIM2) {
    // 读取PPS状态
    uint8_t pps = (HAL_GPIO_ReadPin(PPS_GPIO_Port, PPS_Pin) == GPIO_PIN_SET);
    ms_since_pps = (ms_since_pps + 1) % 1000;

    // TOD串口仿真：PPS上升沿后发送一帧
    if (pps && !prev_pps) {
      ser_len = 0;
      for (uint8_t i = 0; i < TOD_BYTE_COUNT; i++) {
        ser_bits[ser_len++] = 0; // start bit
        for (uint8_t b = 0; b < 8; b++)
          ser_bits[ser_len++] = (tod_frame[i] >> b) & 1;
        ser_bits[ser_len++] = 1; // stop bit
      }
      ser_idx = 0;
    }
    prev_pps = pps;

    // 只在PPS低电平期间输出TOD，PPS高电平期间TOD线保持高
    if (!pps && ser_idx < ser_len) {
      HAL_GPIO_WritePin(TOD_GPIO_Port, TOD_Pin,
                        ser_bits[ser_idx++] ? GPIO_PIN_SET : GPIO_PIN_RESET);
    } else {
      HAL_GPIO_WritePin(TOD_GPIO_Port, TOD_Pin, GPIO_PIN_SET);
    }

    // 前导码/曼彻斯特编码输出到COMPOSITE（可选：如需合成信号）
    if (in_preamble) {
      // 前导码阶段：50%占空比方波
      if (++preamble_half_tick >= MANCHESTER_HALF_MS) {
        preamble_half_tick = 0;
        HAL_GPIO_TogglePin(COMPOSITE_GPIO_Port, COMPOSITE_Pin);
      }
      if (++preamble_ms_count >= PREAMBLE_MS) {
        in_preamble = 0;
        preamble_ms_count = 0;
        bit_index = 0;
        man_tick = 0;
        man_half = 0;
      }
    } else {
      // 数据段：Manchester编码
      Manchester_Update();
    }

    // 合成信号输出：PPS高电平优先，PPS低电平期间为曼彻斯特/前导码
    GPIO_PinState pps_now = HAL_GPIO_ReadPin(PPS_GPIO_Port, PPS_Pin);
    if (pps_now == GPIO_PIN_SET) {
      HAL_GPIO_WritePin(COMPOSITE_GPIO_Port, COMPOSITE_Pin, GPIO_PIN_SET);
    }
    // 否则COMPOSITE已由前导码/曼彻斯特逻辑写入
  }
}
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
