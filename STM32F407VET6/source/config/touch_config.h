/** @file touch_config.h
 * @brief 触觉采样和标定。标定值来自用户提供的最新联合采集工程，换传感器后需重测。
 */
#pragma once
#define TOUCH_SAMPLE_HZ 10000U
#define TOUCH_BLOCK_FRAMES 100U
#define TOUCH_SENSOR_BAUD 460800U
#define TOUCH_PC_BAUD 115200U
/* 保留 100 Hz 块统计；电脑输出最新块 50 Hz，给 115200 的 F/A/S 文本留带宽。 */
#define TOUCH_OUTPUT_MS 20U
#define TOUCH_STALE_MS 300U
#define TOUCH_ZERO_X_RAW 1972.83f
#define TOUCH_ZERO_Y_RAW 1980.44f
#define TOUCH_ZERO_Z_RAW 1969.37f
#define TOUCH_SENS_X_RAW_G 417.19f
#define TOUCH_SENS_Y_RAW_G 416.32f
#define TOUCH_SENS_Z_RAW_G 416.84f
/* 与原示例一致，仅扣除力零偏，不对力矩去零。不是上电自动置零。 */
#define TOUCH_ZERO_FX_N (-28.049325f)
#define TOUCH_ZERO_FY_N (-16.380930f)
#define TOUCH_ZERO_FZ_N (-3.449119f)

/* USART1 ownership: 1=K230 115200, 0=PC F/A/S text. Never both. */
#ifndef TOUCH_USART1_K230
#define TOUCH_USART1_K230 1
#endif

/** @brief PD14 lamp control: high=on, low=off. */
#ifndef TOUCH_LIGHT_CONFIGURED
#define TOUCH_LIGHT_CONFIGURED 1
#endif
#define TOUCH_LIGHT_PORT GPIOD
#define TOUCH_LIGHT_PIN GPIO_PIN_14
#define TOUCH_LIGHT_ACTIVE GPIO_PIN_SET
#define TOUCH_LIGHT_CLOCK_ENABLE() __HAL_RCC_GPIOD_CLK_ENABLE()
/* Camera view excludes the probe. Require stable motor feedback, not a parking coordinate. */
#define TOUCH_STILL_MS 200U
#define TOUCH_EXPOSURE_MS 2000U
#define TOUCH_CAPTURE_TIMEOUT_MS 5000U
#define TOUCH_CAPTURE_TOTAL_MS 8000U
