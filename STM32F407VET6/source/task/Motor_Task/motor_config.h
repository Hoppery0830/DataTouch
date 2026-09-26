/**
 * @file motor_config.h
 * @brief 三轴机械参数与枚举；默认值需与实际导程、减速比、细分和方向一致。
 * @see 本目录 README.md（职责、调用顺序与使用限制）。
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef enum
{
    MOTOR_X = 0,
    MOTOR_Z = 1,
    MOTOR_YAW = 2,
    AXIS_COUNT = 3
} axis_id_t;
typedef struct
{
    float units_per_motor_rev; /* X/Z mm/rev; Yaw degrees per MOTOR rev incl. gearing */
    float min_position, max_position, max_speed, tolerance;
    uint32_t pulses_per_rev; /**< 每电机转的指令脉冲数，须与驱动器细分一致。 */
    uint8_t acceleration;    /**< Emm 加速度参数，不是 mm/s² 或度/s²；0 表示直接启动。 */
    bool reversed;           /**< 同时反转命令方向和反馈方向，使上层坐标保持一致。 */
} axis_config_t;
extern const axis_config_t g_axis_config[AXIS_COUNT];
/* Initial mechanical assumptions: X/Z lead 1mm, 16 microsteps, Yaw direct drive.
 * Validate limits/direction/gearing; auto-run is controlled by APP_MOTOR_SELFTEST. */

/** 固定动作模式：轴运动方式；不是电机的使能/故障状态。 */
typedef enum
{
    ACTION_PRESS = 0,
    ACTION_RUB,
    ACTION_SLIDE,
    ACTION_MODE_COUNT
} motor_action_mode_t;
typedef struct
{
    float z_press_mm; /**< 相对起点的压入位移；默认正向，右移方向须实物确认。 */
    float z_speed_mm_s;
    uint32_t hold_ms; /**< 压入到位后的停顿。 */
    float yaw_amplitude_deg;
    float yaw_speed_deg_s;
    uint16_t rub_cycles; /**< 一轮为起始角-幅度 → 起始角+幅度。 */
    float x_distance_mm;
    float x_speed_mm_s;
    uint32_t segment_timeout_ms;
} motor_action_config_t;
extern const motor_action_config_t g_action_config[ACTION_MODE_COUNT];
