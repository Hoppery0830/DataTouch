/** @file touch_config.h
 * @brief 触觉采样和标定。标定值来自用户提供的最新联合采集工程，换传感器后需重测。
 */
#pragma once
#define TOUCH_SAMPLE_HZ       10000U
#define TOUCH_BLOCK_FRAMES    100U
#define TOUCH_SENSOR_BAUD     460800U
#define TOUCH_PC_BAUD         115200U
/* 保留 100 Hz 块统计；电脑输出最新块 50 Hz，给 115200 的 F/A/S 文本留带宽。 */
#define TOUCH_OUTPUT_MS       20U
#define TOUCH_STALE_MS        300U
#define TOUCH_ZERO_X_RAW      1972.83f
#define TOUCH_ZERO_Y_RAW      1980.44f
#define TOUCH_ZERO_Z_RAW      1969.37f
#define TOUCH_SENS_X_RAW_G    417.19f
#define TOUCH_SENS_Y_RAW_G    416.32f
#define TOUCH_SENS_Z_RAW_G    416.84f
/* 与原示例一致，仅扣除力零偏，不对力矩去零。不是上电自动置零。 */
#define TOUCH_ZERO_FX_N       (-28.049325f)
#define TOUCH_ZERO_FY_N       (-16.380930f)
#define TOUCH_ZERO_FZ_N       (-3.449119f)
