/**
 * @file motor_state_machine.h
 * @brief 运动路径模式与段切换；底层使能、速度/位置模式归 motor 模块。
 * @see 本目录 README.md（职责、调用顺序与使用限制）。
 */
#pragma once
#include "axis_control_drv.h"
#define PATH_MAX_POINTS 8U
/* These are motion paths; motor enable/mode/transport states live in module/motor. */
typedef enum
{
    PATH_POINT_TO_POINT = 0,
    PATH_RECIPROCATING,
    PATH_WAYPOINTS
} path_mode_t;
typedef struct
{
    float target[AXIS_COUNT]; /**< 相对当前已建立原点的绝对目标：X/Z mm，Yaw 度。 */
    float speed[AXIS_COUNT];  /**< 各轴正速度幅值：X/Z mm/s，Yaw 度/s。 */
    uint32_t dwell_ms;        /**< 全部选中轴到位后停留时间，单位 ms。 */
} path_point_t;
typedef struct
{
    path_mode_t mode;
    uint8_t axis_mask, point_count; /**< bit0=X、bit1=Z、bit2=Yaw；最多 8 个有效点。 */
    uint16_t cycles;                /**< 有限轮数；一轮按顺序遍历 point_count 个点。 */
    uint32_t segment_timeout_ms;    /**< 每个点的总时限，含下发等待、运动和停留。 */
    path_point_t points[PATH_MAX_POINTS];
} motion_path_t;
typedef enum
{
    PATH_READY = 0,
    PATH_RUNNING,
    PATH_DONE,
    PATH_ABORTED,
    PATH_FAILED
} path_status_t;
typedef struct
{
    motion_path_t path;
    path_status_t status;
    uint8_t point, sent_mask; /**< 当前点下标及已下发轴位图，防止每节拍重复发位置命令。 */
    uint16_t cycle;
    uint32_t segment_ms, sent_ms[AXIS_COUNT], dwell_start_ms;
    bool dwelling; /**< 当前是否处于到位后的停留阶段；失去到位条件会重置停留。 */
} motion_state_t;
/** @brief 预检整条路径的模式、选中轴、点数、循环数、时间和所有目标；非法路径不部分执行。 */
bool Motion_Validate(const motion_path_t *path);
/** @brief 校验路径及选中轴状态，复制路径并进入运行态；true 表示接受路径，不是执行完毕。 */
bool Motion_Start(motion_state_t *s, const motion_path_t *p, motor_t motors[AXIS_COUNT],
                  uint32_t now);
/** @brief 根据当前点执行结果切换点/轮次；每个控制节拍调用，now 为毫秒。
 * 所有选中轴到位且停留结束后才换点，失败向全部三轴请求停止。 */
void Motion_Update(motion_state_t *s, motor_t motors[AXIS_COUNT], uint32_t now);
/** @brief 将路径置为中止或失败，并请求三轴停止；串口应答/停止超时仍由 motor 模块处理。 */
void Motion_Abort(motion_state_t *s, motor_t motors[AXIS_COUNT], bool failed);
