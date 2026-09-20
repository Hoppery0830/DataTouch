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
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "k230_protocol.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum
{
  MEASURE_IDLE = 0U,
  MEASURE_PRESSING,
  MEASURE_TARGET_REACHED
} MeasureState_t;

typedef enum
{
  MEASURE_EVENT_NONE = 0U,
  MEASURE_EVENT_CONTACT,
  MEASURE_EVENT_TARGET,
  MEASURE_EVENT_READY,
  MEASURE_EVENT_CANCEL,
  MEASURE_EVENT_TIMEOUT
} MeasurementEvent_t;

typedef struct
{
  uint16_t hall_mv;
  float distance_mm;
} HallCalibrationPoint;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define ADC_VREF_MV                 3300U /* ADC reference voltage in mV. */
#define ADC_FULL_SCALE              4095U /* 12-bit ADC maximum code. */
#ifndef HALL_ONLY_TEST
#define HALL_ONLY_TEST                0U /* Set to 1 only for PA1-only diagnosis. */
#endif
#define BASELINE_SAMPLE_COUNT         50U /* Startup no-load baseline samples. */
#define PRESSURE_MIN_MV              200U /* Voltage mapped to 100.0% index. */
#define HALL_AVERAGE_SAMPLE_COUNT      8U /* Lightweight Hall average window. */
#define HALL_TEST_PERIOD_MS          100U /* Hall-only UART output period. */
#define PRESSURE_INDEX_MAX_X10      1000U /* 100.0%, stored in 0.1% units. */
#define PRESSURE_CURVE_STEP_X10       10U /* LUT spacing: 1.0% linear input. */
#define CONTACT_THRESHOLD_X10        150U /* 15.0%: contact threshold. */
#define TARGET_THRESHOLD_X10         850U /* 85.0%: target threshold. */
#define RELEASE_THRESHOLD_X10         50U /* 5.0%: release threshold. */
#define CONTACT_CONFIRM_MS              5U /* Contact confirmation duration. */
#define TARGET_CONFIRM_MS               5U /* Target confirmation duration. */
#define CANCEL_CONFIRM_MS              10U /* Cancel duration below release. */
#define TARGET_TIMEOUT_MS            2000U /* Temporary manual-test timeout. */
#define SAMPLE_PERIOD_MS               1U /* 1 ms internal sampling period. */
#define SOFTNESS_SAMPLE_HZ   (1000U / SAMPLE_PERIOD_MS)
#define TIM2_PRESCALER                71U /* 72 MHz / (71 + 1) = 1 MHz. */
#define TIM2_PERIOD                  999U /* 1 MHz / (999 + 1) = 1 kHz. */
#define MOVING_AVERAGE_SIZE            4U /* Four-point ADC sliding average. */
#define RELEASE_STABLE_MS            200U /* Required release stability time. */
#define RELEASE_STABLE_COUNT ((SOFTNESS_SAMPLE_HZ * RELEASE_STABLE_MS) / 1000U)
#define BASELINE_WARNING_MV         2500U /* Warn when startup baseline is low. */
#define SOFT_TIME_MIN_MS              40U /* Temporary lower softness bound. */
#define SOFT_TIME_MAX_MS             300U /* Temporary upper softness bound. */
#define SOFTNESS_DATA_LOG_ENABLE       1U /* 1: debug logging, 0: silent run. */
#define UART_LOG_PERIOD_MS             20U /* 50 Hz ordinary UART logging. */
#define CAL_DEBUG_LOG_PERIOD_MS       300U /* Baseline diagnostic log period. */
#define CAL_DIAGNOSTIC_STABLE_SPAN_ADC 64U /* Diagnostic only; never gates exit. */
#define CONTACT_CONFIRM_COUNT (CONTACT_CONFIRM_MS / SAMPLE_PERIOD_MS)
#define TARGET_CONFIRM_COUNT  (TARGET_CONFIRM_MS / SAMPLE_PERIOD_MS)
#define CANCEL_CONFIRM_COUNT  (CANCEL_CONFIRM_MS / SAMPLE_PERIOD_MS)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

TIM_HandleTypeDef htim2;

/* USER CODE BEGIN PV */
static uint32_t baseline_adc = 0U;
static uint16_t adc_raw = 0U;
static uint32_t adc_avg = 0U;
static uint32_t voltage_mv = 0U;
static uint16_t hall_adc_raw = 0U;
static uint32_t hall_adc_avg = 0U;
static uint32_t hall_mv = 0U;
static float hall_distance_mm = 0.0f;
static uint32_t baseline_mv = 0U;
static uint16_t pressure_linear_x10 = 0U;
/* Nonlinear mapped value; this variable does not add a time-domain filter. */
static uint16_t pressure_filtered_x10 = 0U;
static uint16_t pressure_index_x10 = 0U;
static uint32_t release_stable_count = 0U;
static uint32_t contact_candidate_time_ms = 0U;
static uint32_t target_candidate_time_ms = 0U;
static uint32_t contact_confirm_count = 0U;
static uint32_t target_confirm_count = 0U;
static uint32_t cancel_confirm_count = 0U;
static MeasureState_t measure_state = MEASURE_IDLE;
static uint16_t adc_history[MOVING_AVERAGE_SIZE] = {0U};
static uint32_t adc_history_sum = 0U;
static uint32_t adc_history_index = 0U;
static uint32_t adc_configured_channel = UINT32_MAX;
static uint32_t last_log_time_ms = 0U;
static uint8_t adc_success_status_reported = 0U;
#if !HALL_ONLY_TEST
static uint8_t baseline_calibration_active = 0U;
static uint32_t baseline_debug_sum = 0U;
static uint32_t baseline_debug_count = 0U;
static uint16_t baseline_debug_min = UINT16_MAX;
static uint16_t baseline_debug_max = 0U;
static uint32_t baseline_debug_last_log_ms = 0U;
#endif
uint8_t uart_tx_queue[UART_TX_QUEUE_SIZE] = {0U};
volatile uint16_t uart_tx_head = 0U;
volatile uint16_t uart_tx_tail = 0U;

volatile uint32_t pending_samples = 0U;
volatile uint32_t sample_counter = 0U;
SoftnessResult_t softness_result = {0U};
char uart_tx_buffer[224];

/*
 * Manual rough calibration for the present magnet, polarity, Hall mounting,
 * and mechanical axis.  This is not a DRV5055A2 universal formula.
 * Points are ordered by hall_mv so the interpolation remains table-driven.
 */
static const HallCalibrationPoint hall_table[] =
{
  {200U,  0.0f},
  {234U,  1.0f},
  {454U,  2.0f},
  {674U,  4.0f},
  {1128U, 6.0f},
  {1540U, 10.0f},
  {1654U, 21.0f},
  {1698U, 28.0f},
  {1716U, 30.0f}
};
#define HALL_TABLE_SIZE (sizeof(hall_table) / sizeof(hall_table[0]))

/*
 * Integer LUT for the normalized curve:
 *
 *   pressure_filtered = 1000 * (pressure_linear / 1000)^1.5
 *
 * The entries are spaced every 10 x10 units (1.0% linear pressure).  The
 * runtime lookup linearly interpolates between adjacent entries, so the
 * output remains monotonic without using floating-point arithmetic.
 */
static const uint16_t pressure_curve_lut[] =
{
  0U,   1U,   3U,   5U,   8U,  11U,  15U,  19U,  23U,  27U,
 32U,  36U,  42U,  47U,  52U,  58U,  64U,  70U,  76U,  83U,
 89U,  96U, 103U, 110U, 118U, 125U, 133U, 140U, 148U, 156U,
164U, 173U, 181U, 190U, 198U, 207U, 216U, 225U, 234U, 244U,
253U, 263U, 272U, 282U, 292U, 302U, 312U, 322U, 333U, 343U,
354U, 364U, 375U, 386U, 397U, 408U, 419U, 430U, 442U, 453U,
465U, 476U, 488U, 500U, 512U, 524U, 536U, 548U, 561U, 573U,
586U, 598U, 611U, 624U, 637U, 650U, 663U, 676U, 689U, 702U,
716U, 729U, 743U, 756U, 770U, 784U, 798U, 811U, 826U, 840U,
854U, 868U, 882U, 897U, 911U, 926U, 941U, 955U, 970U, 985U,
1000U
};
#define PRESSURE_CURVE_LUT_SIZE \
  (sizeof(pressure_curve_lut) / sizeof(pressure_curve_lut[0]))

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */
static uint16_t ADC_Read_Channel(uint32_t channel);
static uint16_t Read_ADC_Single(void);
#if !HALL_ONLY_TEST
static uint32_t Read_ADC_Average(uint32_t sample_count);
#endif
static uint16_t Read_ADC_Filtered(void);
static void Initialize_ADC_Filter(void);
static uint32_t ADC_To_mV(uint32_t adc_value);
static uint16_t Hall_ADC_ReadRaw(void);
static uint16_t Hall_ADC_ReadAverage(uint32_t sample_count);
static void Hall_Update(void);
static uint32_t Hall_FloatMmToCenti(float value_mm);
static int Hall_Format_Distance(char *buffer, size_t buffer_size);
#if !HALL_ONLY_TEST
static uint32_t Calibrate_Baseline(void);
static void Print_Calibration_Debug(uint16_t adc_value, const char *state);
#endif
#if HALL_ONLY_TEST
static void Hall_Only_Test_Loop(void);
#endif
static void Print_ADC_Status(const char *operation,
                             uint32_t channel,
                             HAL_StatusTypeDef status);
static uint16_t Calculate_Pressure_Linear(uint32_t voltage_value_mv,
                                          uint32_t baseline_value_mv);
static uint16_t Apply_Pressure_Curve(uint16_t pressure_linear_value_x10);
static uint8_t Calculate_Softness(uint32_t time_ms);
static void Softness_ResetMeasurement(void);
static MeasurementEvent_t Update_Measurement_State(uint32_t pressure_x10,
                                                   uint32_t time_ms);
static uint8_t Fetch_Pending_Sample(uint32_t *sample_time_ms);
static void UART_Queue_Bytes(const uint8_t *data, uint16_t length);
static void UART_Send_Buffer(int length);
static void Print_Debug_Data(uint32_t time_ms);
static void Print_Event(MeasurementEvent_t event, uint32_t event_time_ms);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_ADC1_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  K230_Protocol_Init(&huart2);

  if (HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

#if HALL_ONLY_TEST
  /* Temporary diagnosis: do not touch PA0 or execute pressure business logic. */
  Hall_Only_Test_Loop();
#else
  /* The startup phase assumes the sensor is unloaded. */
  baseline_adc = Calibrate_Baseline();
  baseline_mv = ADC_To_mV(baseline_adc);

  Softness_Init();
  Hall_Update();

  pending_samples = 0U;
  sample_counter = 0U;
  if (HAL_TIM_Base_Start_IT(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
#endif

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
#if !HALL_ONLY_TEST
    Softness_Process();
#endif
    K230_Protocol_Process();
  }
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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

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
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_71CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

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
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
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
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* The HW-269 control signal is low before PB0 becomes an output. */
  HAL_GPIO_WritePin(LIGHT_CTRL_GPIO_Port, LIGHT_CTRL_Pin, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = LIGHT_CTRL_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LIGHT_CTRL_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
static void MX_TIM2_Init(void)
{
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = TIM2_PRESCALER;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = TIM2_PERIOD;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
}

static void Print_ADC_Status(const char *operation,
                             uint32_t channel,
                             HAL_StatusTypeDef status)
{
  /* Keep error handling unchanged, but suppress non-parameter UART text. */
  (void)operation;
  (void)channel;
  (void)status;
}

static uint16_t ADC_Read_Channel(uint32_t channel)
{
  ADC_ChannelConfTypeDef channel_config = {0};
  uint16_t adc_value = 0U;
  uint8_t channel_changed =
      (adc_configured_channel != channel) ? 1U : 0U;

  if (channel_changed != 0U)
  {
    channel_config.Channel = channel;
    channel_config.Rank = ADC_REGULAR_RANK_1;
    channel_config.SamplingTime = ADC_SAMPLETIME_71CYCLES_5;
    HAL_StatusTypeDef config_status =
        HAL_ADC_ConfigChannel(&hadc1, &channel_config);
    if (config_status != HAL_OK)
    {
      Print_ADC_Status("HAL_ADC_ConfigChannel", channel, config_status);
      Error_Handler();
      return 0U;
    }
    adc_configured_channel = channel;
  }

  /*
   * The first conversion after changing PA0/PA1 can contain charge from the
   * previous channel.  If it is discarded, a NEW HAL_ADC_Start() is required
   * before polling again.  The previous implementation polled twice after a
   * single start, which timed out in single-conversion mode.
   */
  for (uint32_t conversion_index = 0U;
       conversion_index < ((channel_changed != 0U) ? 2U : 1U);
       ++conversion_index)
  {
    HAL_StatusTypeDef start_status = HAL_ADC_Start(&hadc1);
    if (start_status != HAL_OK)
    {
      Print_ADC_Status("HAL_ADC_Start", channel, start_status);
      Error_Handler();
      return 0U;
    }

    if (adc_success_status_reported == 0U)
    {
      Print_ADC_Status("HAL_ADC_Start", channel, HAL_OK);
    }

    HAL_StatusTypeDef poll_status = HAL_ADC_PollForConversion(&hadc1, 100U);
    if (poll_status != HAL_OK)
    {
      Print_ADC_Status("HAL_ADC_PollForConversion", channel, poll_status);
      (void)HAL_ADC_Stop(&hadc1);
      Error_Handler();
      return 0U;
    }

    adc_value = (uint16_t)HAL_ADC_GetValue(&hadc1);

    HAL_StatusTypeDef stop_status = HAL_ADC_Stop(&hadc1);
    if (stop_status != HAL_OK)
    {
      Print_ADC_Status("HAL_ADC_Stop", channel, stop_status);
      Error_Handler();
      return 0U;
    }

    if ((adc_success_status_reported == 0U) &&
        (conversion_index == 0U))
    {
      Print_ADC_Status("HAL_ADC_PollForConversion", channel, HAL_OK);
      adc_success_status_reported = 1U;
    }
  }

  return adc_value;
}

static uint16_t Read_ADC_Single(void)
{
  return ADC_Read_Channel(ADC_CHANNEL_0);
}

#if !HALL_ONLY_TEST
static uint32_t Read_ADC_Average(uint32_t sample_count)
{
  uint32_t adc_sum = 0U;

  if (sample_count == 0U)
  {
    Error_Handler();
    return 0U;
  }

  for (uint32_t i = 0U; i < sample_count; ++i)
  {
    uint16_t sample = Read_ADC_Single();
    adc_raw = sample;
    adc_sum += sample;

    if (baseline_calibration_active != 0U)
    {
      uint32_t now_ms;

      baseline_debug_sum += sample;
      baseline_debug_count = i + 1U;
      if (sample < baseline_debug_min)
      {
        baseline_debug_min = sample;
      }
      if (sample > baseline_debug_max)
      {
        baseline_debug_max = sample;
      }

      now_ms = HAL_GetTick();
      if (((now_ms - baseline_debug_last_log_ms) >=
           CAL_DEBUG_LOG_PERIOD_MS) ||
          (baseline_debug_count == sample_count))
      {
        Print_Calibration_Debug(sample, "COLLECTING");
        baseline_debug_last_log_ms = now_ms;
      }
    }
  }

  return (adc_sum + (sample_count / 2U)) / sample_count;
}
#endif

static uint16_t Read_ADC_Filtered(void)
{
  uint16_t new_adc = Read_ADC_Single();
  uint16_t filtered_adc;

  adc_raw = new_adc;

  adc_history_sum -= adc_history[adc_history_index];
  adc_history[adc_history_index] = new_adc;
  adc_history_sum += new_adc;

  ++adc_history_index;
  if (adc_history_index >= MOVING_AVERAGE_SIZE)
  {
    adc_history_index = 0U;
  }

  filtered_adc = (uint16_t)(
      (adc_history_sum + (MOVING_AVERAGE_SIZE / 2U)) /
      MOVING_AVERAGE_SIZE);
  return filtered_adc;
}

static void Initialize_ADC_Filter(void)
{
  for (uint32_t i = 0U; i < MOVING_AVERAGE_SIZE; ++i)
  {
    adc_history[i] = (uint16_t)baseline_adc;
  }

  adc_history_sum = baseline_adc * MOVING_AVERAGE_SIZE;
  adc_history_index = 0U;
}

static uint32_t ADC_To_mV(uint32_t adc_value)
{
  return (adc_value * ADC_VREF_MV + (ADC_FULL_SCALE / 2U)) /
         ADC_FULL_SCALE;
}

float Hall_GetDistanceMm(uint16_t hall_millivolts)
{
  uint32_t lower_index = 0U;

  /* A physical distance cannot be negative below the first calibration point. */
  if (hall_millivolts <= hall_table[0U].hall_mv)
  {
    return 0.0f;
  }

  /* Use the nearest endpoint segment for high-side extrapolation. */
  if (hall_millivolts >= hall_table[HALL_TABLE_SIZE - 1U].hall_mv)
  {
    lower_index = HALL_TABLE_SIZE - 2U;
  }
  else if (hall_millivolts > hall_table[0U].hall_mv)
  {
    for (uint32_t i = 0U; i < (HALL_TABLE_SIZE - 1U); ++i)
    {
      if (hall_millivolts <= hall_table[i + 1U].hall_mv)
      {
        lower_index = i;
        break;
      }
    }
  }

  {
    const HallCalibrationPoint *lower = &hall_table[lower_index];
    const HallCalibrationPoint *upper = &hall_table[lower_index + 1U];
    float voltage_span = (float)(upper->hall_mv - lower->hall_mv);
    float distance_span = upper->distance_mm - lower->distance_mm;

    return lower->distance_mm +
           ((float)((int32_t)hall_millivolts -
                    (int32_t)lower->hall_mv) *
            distance_span / voltage_span);
  }
}

static uint32_t Hall_FloatMmToCenti(float value_mm)
{
  if (value_mm <= 0.0f)
  {
    return 0U;
  }

  return (uint32_t)(value_mm * 100.0f + 0.5f);
}

static int Hall_Format_Distance(char *buffer, size_t buffer_size)
{
  uint32_t distance_centi = Hall_FloatMmToCenti(hall_distance_mm);

  return snprintf(
      buffer,
      buffer_size,
      "%lu.%02lu mm",
      (unsigned long)(distance_centi / 100UL),
      (unsigned long)(distance_centi % 100UL));
}

static uint16_t Hall_ADC_ReadRaw(void)
{
  hall_adc_raw = ADC_Read_Channel(ADC_CHANNEL_1);
  return hall_adc_raw;
}

static uint16_t Hall_ADC_ReadAverage(uint32_t sample_count)
{
  uint32_t adc_sum = 0U;

  if (sample_count == 0U)
  {
    Error_Handler();
    return 0U;
  }

  for (uint32_t i = 0U; i < sample_count; ++i)
  {
    adc_sum += Hall_ADC_ReadRaw();
  }

  return (uint16_t)((adc_sum + (sample_count / 2U)) / sample_count);
}

static void Hall_Update(void)
{
  /* Hall voltage is converted using only the manual calibration table. */
  hall_adc_avg = Hall_ADC_ReadAverage(HALL_AVERAGE_SAMPLE_COUNT);
  hall_mv = ADC_To_mV(hall_adc_avg);
  hall_distance_mm = Hall_GetDistanceMm((uint16_t)hall_mv);
}

#if !HALL_ONLY_TEST
static void Print_Calibration_Debug(uint16_t adc_value, const char *state)
{
  /* Keep the calibration calculations, but suppress diagnostic UART output. */
  (void)adc_value;
  (void)state;
}

static uint32_t Calibrate_Baseline(void)
{
  uint32_t result;

  baseline_debug_sum = 0U;
  baseline_debug_count = 0U;
  baseline_debug_min = UINT16_MAX;
  baseline_debug_max = 0U;
  baseline_debug_last_log_ms = HAL_GetTick();

  baseline_calibration_active = 1U;
  result = Read_ADC_Average(BASELINE_SAMPLE_COUNT);
  baseline_calibration_active = 0U;
  Print_Calibration_Debug(adc_raw, "COMPLETE");

  return result;
}
#endif

#if HALL_ONLY_TEST
static void Hall_Only_Test_Loop(void)
{
  while (1)
  {
    char distance_text[48];
    int length;
    int distance_length;

    Hall_Update();
    distance_length = Hall_Format_Distance(
        distance_text,
        sizeof(distance_text));
    length = snprintf(
        uart_tx_buffer,
        sizeof(uart_tx_buffer),
        "Distance=%s\r\n",
        (distance_length >= 0) ? distance_text : "FORMAT_ERROR");
    UART_Send_Buffer(length);
    HAL_Delay(HALL_TEST_PERIOD_MS);
  }
}
#endif

static uint16_t Calculate_Pressure_Linear(uint32_t voltage_value_mv,
                                          uint32_t baseline_value_mv)
{
  uint32_t pressure_x10;

  /* Prevent an invalid baseline from creating an unsigned divide-by-zero. */
  if (baseline_value_mv <= PRESSURE_MIN_MV)
  {
    return 0U;
  }

  if (voltage_value_mv >= baseline_value_mv)
  {
    return 0U;
  }

  if (voltage_value_mv <= PRESSURE_MIN_MV)
  {
    return PRESSURE_INDEX_MAX_X10;
  }

  pressure_x10 =
      (baseline_value_mv - voltage_value_mv) * PRESSURE_INDEX_MAX_X10 /
      (baseline_value_mv - PRESSURE_MIN_MV);

  if (pressure_x10 > PRESSURE_INDEX_MAX_X10)
  {
    return (uint16_t)PRESSURE_INDEX_MAX_X10;
  }

  return (uint16_t)pressure_x10;
}

static uint16_t Apply_Pressure_Curve(uint16_t pressure_linear_value_x10)
{
  uint32_t lut_index;
  uint32_t lut_remainder;
  uint32_t lower_value;
  uint32_t upper_value;
  uint32_t mapped_value;

  if (pressure_linear_value_x10 >= PRESSURE_INDEX_MAX_X10)
  {
    return (uint16_t)PRESSURE_INDEX_MAX_X10;
  }

  if (pressure_linear_value_x10 == 0U)
  {
    return 0U;
  }

  lut_index = pressure_linear_value_x10 / PRESSURE_CURVE_STEP_X10;
  lut_remainder = pressure_linear_value_x10 % PRESSURE_CURVE_STEP_X10;
  if (lut_index >= (PRESSURE_CURVE_LUT_SIZE - 1U))
  {
    return (uint16_t)PRESSURE_INDEX_MAX_X10;
  }

  lower_value = pressure_curve_lut[lut_index];
  upper_value = pressure_curve_lut[lut_index + 1U];
  mapped_value = lower_value +
                 ((upper_value - lower_value) * lut_remainder +
                  (PRESSURE_CURVE_STEP_X10 / 2U)) /
                 PRESSURE_CURVE_STEP_X10;

  if (mapped_value > PRESSURE_INDEX_MAX_X10)
  {
    return (uint16_t)PRESSURE_INDEX_MAX_X10;
  }

  return (uint16_t)mapped_value;
}

/*
 * PROVISIONAL ONLY: this is a relative softness mapping, not a calibrated
 * physical measurement. SOFT_TIME_MIN_MS and SOFT_TIME_MAX_MS are temporary
 * engineering parameters and must be calibrated with controlled fabric
 * experiments before they are used as a formal softness standard.
 *
 * Manual pressing results are strongly affected by pressing speed, so the
 * current result is suitable only for comparison within the present test
 * setup and must not be treated as a final fabric calibration value.
 */
static uint8_t Calculate_Softness(uint32_t time_ms)
{
  if (time_ms <= SOFT_TIME_MIN_MS)
  {
    return 1U;
  }

  if (time_ms >= SOFT_TIME_MAX_MS)
  {
    return 100U;
  }

  return (uint8_t)(1U +
                   (time_ms - SOFT_TIME_MIN_MS) * 99U /
                   (SOFT_TIME_MAX_MS - SOFT_TIME_MIN_MS));
}

static void Softness_ResetMeasurement(void)
{
  contact_candidate_time_ms = 0U;
  target_candidate_time_ms = 0U;
  contact_confirm_count = 0U;
  target_confirm_count = 0U;
  cancel_confirm_count = 0U;
  release_stable_count = 0U;
  measure_state = MEASURE_IDLE;

  /* Keep baseline_mv and all ADC/TIM2 configuration unchanged. */
  softness_result.contact_time_ms = 0U;
  softness_result.target_time_ms = 0U;
  softness_result.softness_time_ms = 0U;
  softness_result.pressure_index_x10 = 0U;
  softness_result.softness = 0U;
  softness_result.valid = 0U;
}

static MeasurementEvent_t Update_Measurement_State(uint32_t pressure_x10,
                                                   uint32_t time_ms)
{
  MeasurementEvent_t event = MEASURE_EVENT_NONE;

  switch (measure_state)
  {
    case MEASURE_IDLE:
      if (pressure_x10 >= CONTACT_THRESHOLD_X10)
      {
        if (contact_confirm_count == 0U)
        {
          contact_candidate_time_ms = time_ms;
        }

        ++contact_confirm_count;
        if (contact_confirm_count >= CONTACT_CONFIRM_COUNT)
        {
          /* Keep the first threshold crossing, not the fifth sample. */
          softness_result.contact_time_ms = contact_candidate_time_ms;
          softness_result.target_time_ms = 0U;
          softness_result.softness_time_ms = 0U;
          softness_result.pressure_index_x10 = (uint16_t)pressure_x10;
          softness_result.softness = 0U;
          softness_result.valid = 0U;

          contact_candidate_time_ms = 0U;
          contact_confirm_count = 0U;
          target_candidate_time_ms = 0U;
          target_confirm_count = 0U;
          cancel_confirm_count = 0U;
          release_stable_count = 0U;
          measure_state = MEASURE_PRESSING;
          event = MEASURE_EVENT_CONTACT;
        }
      }
      else
      {
        contact_candidate_time_ms = 0U;
        contact_confirm_count = 0U;
      }
      break;

    case MEASURE_PRESSING:
      if (pressure_x10 < RELEASE_THRESHOLD_X10)
      {
        ++cancel_confirm_count;
        target_candidate_time_ms = 0U;
        target_confirm_count = 0U;

        if (cancel_confirm_count >= CANCEL_CONFIRM_COUNT)
        {
          Softness_ResetMeasurement();
          event = MEASURE_EVENT_CANCEL;
          break;
        }
      }
      else
      {
        cancel_confirm_count = 0U;
      }

      if (pressure_x10 >= TARGET_THRESHOLD_X10)
      {
        if (target_confirm_count == 0U)
        {
          target_candidate_time_ms = time_ms;
        }

        ++target_confirm_count;
        if (target_confirm_count >= TARGET_CONFIRM_COUNT)
        {
          /* Keep the first threshold crossing, not the fifth sample. */
          softness_result.target_time_ms = target_candidate_time_ms;

          /* Unsigned tick subtraction is wrap-safe for one short measurement. */
          softness_result.softness_time_ms =
              softness_result.target_time_ms - softness_result.contact_time_ms;
          softness_result.softness =
              Calculate_Softness(softness_result.softness_time_ms);
          softness_result.pressure_index_x10 = (uint16_t)pressure_x10;
          softness_result.valid = 1U;
          target_candidate_time_ms = 0U;
          target_confirm_count = 0U;
          measure_state = MEASURE_TARGET_REACHED;
          release_stable_count = 0U;
          event = MEASURE_EVENT_TARGET;
          break;
        }
      }
      else
      {
        target_candidate_time_ms = 0U;
        target_confirm_count = 0U;
      }

      /*
       * TODO: TARGET_TIMEOUT_MS must be recalibrated after constant-speed
       * mechanical pressing is implemented.
       */
      if ((time_ms - softness_result.contact_time_ms) > TARGET_TIMEOUT_MS)
      {
        Softness_ResetMeasurement();
        event = MEASURE_EVENT_TIMEOUT;
      }
      break;

    case MEASURE_TARGET_REACHED:
      if (pressure_x10 < RELEASE_THRESHOLD_X10)
      {
        if (release_stable_count < RELEASE_STABLE_COUNT)
        {
          ++release_stable_count;
        }

        if (release_stable_count >= RELEASE_STABLE_COUNT)
        {
          Softness_ResetMeasurement();
          event = MEASURE_EVENT_READY;
        }
      }
      else
      {
        release_stable_count = 0U;
      }
      break;

    default:
      Softness_ResetMeasurement();
      break;
  }

  return event;
}

static void UART_Send_Buffer(int length)
{
  if ((length < 0) || (length >= (int)sizeof(uart_tx_buffer)))
  {
    Error_Handler();
    return;
  }

  if (length > 0)
  {
    UART_Queue_Bytes((const uint8_t *)uart_tx_buffer, (uint16_t)length);
  }
}

static void UART_Queue_Bytes(const uint8_t *data, uint16_t length)
{
  uint16_t next_head;

  /* Keep the critical section short; no blocking UART operation is used. */
  __disable_irq();
  for (uint16_t i = 0U; i < length; ++i)
  {
    next_head = (uint16_t)(uart_tx_head + 1U);
    if (next_head >= UART_TX_QUEUE_SIZE)
    {
      next_head = 0U;
    }

    /* The configured queue is sized for the normal 50 Hz debug stream. */
    if (next_head == uart_tx_tail)
    {
      break;
    }

    uart_tx_queue[uart_tx_head] = data[i];
    uart_tx_head = next_head;
  }
  __HAL_UART_ENABLE_IT(&huart1, UART_IT_TXE);
  __enable_irq();
}

void Debug_UART1_Write(const uint8_t *data, uint16_t length)
{
  if ((data != NULL) && (length > 0U))
  {
    UART_Queue_Bytes(data, length);
  }
}

static void Print_Debug_Data(uint32_t time_ms)
{
  char distance_text[48];
  int distance_length = Hall_Format_Distance(
      distance_text,
      sizeof(distance_text));
  int length = snprintf(
      uart_tx_buffer,
      sizeof(uart_tx_buffer),
      "Pressure=%u.%u%%, Distance=%s\r\n",
      (unsigned int)(pressure_index_x10 / 10U),
      (unsigned int)(pressure_index_x10 % 10U),
      (distance_length >= 0) ? distance_text : "FORMAT_ERROR");

  (void)time_ms;
  UART_Send_Buffer(length);
}

static void Print_Event(MeasurementEvent_t event, uint32_t event_time_ms)
{
  /* Event calculation remains active; event text is intentionally suppressed. */
  (void)event;
  (void)event_time_ms;
}

static uint8_t Fetch_Pending_Sample(uint32_t *sample_time_ms)
{
  uint8_t has_sample = 0U;

  __disable_irq();
  if (pending_samples > 0U)
  {
    --pending_samples;
    /* The oldest queued tick is counter - remaining queued samples. */
    *sample_time_ms = sample_counter - pending_samples;
    has_sample = 1U;
  }
  __enable_irq();

  return has_sample;
}

void Softness_Init(void)
{
  last_log_time_ms = 0U;
  softness_result.baseline_mv = (uint16_t)baseline_mv;
  Softness_ResetMeasurement();

  Initialize_ADC_Filter();
}

void Softness_Process(void)
{
  uint32_t sample_time_ms;
  MeasurementEvent_t event;

  if (Fetch_Pending_Sample(&sample_time_ms) == 0U)
  {
    return;
  }

  adc_avg = Read_ADC_Filtered();
  voltage_mv = ADC_To_mV(adc_avg);

  /* This is a relative pressure index, not calibrated physical force. */
  pressure_linear_x10 =
      Calculate_Pressure_Linear(voltage_mv, baseline_mv);
  pressure_filtered_x10 = Apply_Pressure_Curve(pressure_linear_x10);
  pressure_index_x10 = pressure_filtered_x10;
  Hall_Update();
  event = Update_Measurement_State(pressure_index_x10, sample_time_ms);

  /* Events are sent regardless of the optional continuous data log. */
  Print_Event(event, sample_time_ms);

#if SOFTNESS_DATA_LOG_ENABLE
  if ((sample_time_ms - last_log_time_ms) >= UART_LOG_PERIOD_MS)
  {
    Print_Debug_Data(sample_time_ms);
    last_log_time_ms = sample_time_ms;
  }
#endif
}

uint8_t Softness_IsValid(void)
{
  return softness_result.valid;
}

uint8_t Softness_GetValue(void)
{
  return softness_result.softness;
}

SoftnessResult_t Softness_GetResult(void)
{
  return softness_result;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM2)
  {
    ++sample_counter;
    if (pending_samples < UINT32_MAX)
    {
      ++pending_samples;
    }
  }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
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
