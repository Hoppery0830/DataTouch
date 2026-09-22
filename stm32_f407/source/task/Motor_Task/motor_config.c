/**
 * @file motor_config.c
 * @brief 三轴机械参数与枚举；默认值需与实际导程、减速比、细分和方向一致。
 * @see 本目录 README.md（职责、调用顺序与使用限制）。
 */
#include "motor_config.h"
const axis_config_t g_axis_config[AXIS_COUNT] = {
    {1.0f, 0.0f, 50.0f, 8.0f, 0.02f, 3200, 10, false},
    {1.0f, 0.0f, 50.0f, 8.0f, 0.02f, 3200, 10, false},
    {360.0f, -180.0f, 180.0f, 90.0f, 0.2f, 3200, 10, false}};

/* 位置控制的初始试验参数。没有力反馈，不代表恒定压力；不自动执行。 */
const motor_action_config_t g_action_config[ACTION_MODE_COUNT] = {
    [ACTION_PRESS] = {.z_press_mm = 3.0f,
                      .z_speed_mm_s = 1.0f,
                      .hold_ms = 300,
                      .segment_timeout_ms = 15000},
    [ACTION_RUB] = {.z_press_mm = 3.0f,
                    .z_speed_mm_s = 1.0f,
                    .hold_ms = 300,
                    .yaw_amplitude_deg = 10.0f,
                    .yaw_speed_deg_s = 10.0f,
                    .rub_cycles = 3,
                    .segment_timeout_ms = 15000},
    [ACTION_SLIDE] = {.z_press_mm = 3.0f,
                      .z_speed_mm_s = 1.0f,
                      .hold_ms = 300,
                      .x_distance_mm = 10.0f,
                      .x_speed_mm_s = 1.0f,
                      .segment_timeout_ms = 15000}};
