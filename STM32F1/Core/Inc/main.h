/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
typedef struct
{
  uint32_t contact_time_ms;
  uint32_t target_time_ms;
  uint32_t softness_time_ms;

  uint16_t baseline_mv;
  uint16_t pressure_index_x10;

  uint8_t softness;
  uint8_t valid;
} SoftnessResult_t;

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */
#define UART_TX_QUEUE_SIZE 512U
extern uint8_t uart_tx_queue[UART_TX_QUEUE_SIZE];
extern volatile uint16_t uart_tx_head;
extern volatile uint16_t uart_tx_tail;

#define LIGHT_CTRL_Pin GPIO_PIN_0
#define LIGHT_CTRL_GPIO_Port GPIOB

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
void Softness_Init(void);
void Softness_Process(void);
uint8_t Softness_IsValid(void);
uint8_t Softness_GetValue(void);
SoftnessResult_t Softness_GetResult(void);
extern SoftnessResult_t softness_result;

/* Convert Hall voltage to distance using the manual LUT interpolation. */
float Hall_GetDistanceMm(uint16_t hall_mv);
void Debug_UART1_Write(const uint8_t *data, uint16_t length);

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
