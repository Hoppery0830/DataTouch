/**
 * @file motor_task.h
 * @brief 电机任务入口、命令分发和状态快照；统一推进通信、路径与自检。
 * @see 本目录 README.md（职责、调用顺序与使用限制）。
 */
#pragma once
#include "motor_state_machine.h"
#include "motor_action.h"
#include "remote_protocol.h"
typedef enum
{
    MOTOR_CMD_PATH = 0,
    MOTOR_CMD_ENABLE,
    MOTOR_CMD_ORIGIN,
    MOTOR_CMD_CLEAR_FAULT,
    MOTOR_CMD_ACTION
} motor_command_kind_t;
typedef enum
{
    SELFTEST_DISABLED = 0,
    SELFTEST_WAIT_ONLINE,
    SELFTEST_ENABLE,
    SELFTEST_ZERO,
    SELFTEST_PATH,
    SELFTEST_PAUSE,
    SELFTEST_STOPPED,
    SELFTEST_FAILED
} selftest_state_t;
/** @brief 上电固定动作流程；完成/停止/失败后均不自动重启。 */
typedef enum
{
    STARTUP_DISABLED = 0,
    STARTUP_WAIT_ONLINE,
    STARTUP_ENABLE,
    STARTUP_ZERO,
    STARTUP_ACTION,
    STARTUP_DONE,
    STARTUP_STOPPED,
    STARTUP_FAILED
} motor_startup_state_t;

typedef struct
{
    motor_command_kind_t
        kind; /**< 命令类型；PATH 用 path，ACTION 用 action_mode，其余用 axis/enable。 */
    axis_id_t axis;
    bool enable;
    motor_action_mode_t action_mode; /**< MOTOR_CMD_ACTION 使用的固定模式。 */
    motion_path_t path;
} motor_command_t;
typedef struct
{
    float position[AXIS_COUNT]; /**< X/Z 为 mm，Yaw 为度；读取时同时检查 online。 */
    motor_fault_t fault[AXIS_COUNT];
    bool online[AXIS_COUNT], enabled[AXIS_COUNT], origin_valid[AXIS_COUNT], stop_failed[AXIS_COUNT];
    remote_status_t remote; /**< STM32 权威远程状态，仅用于反馈。 */
    path_status_t path_status;
    motor_action_mode_t action_mode;
    motor_action_status_t action_status;
    motor_action_stage_t action_stage;
    uint8_t point;
    uint16_t cycle;
    uint32_t control_ticks, missed_ticks, max_iteration_us, rejected_commands, processed_commands;
    bool last_command_ok; /**< 最近一次任务分发是否接受；不是驱动器 ACK 或到位结果。 */
    motor_startup_state_t startup_state; /**< 上电准备与单次动作状态。 */
    selftest_state_t selftest_state;
    uint32_t selftest_rounds; /**< 已完成目标→零点的自检轮数。 */
} motor_task_status_t;
/* Task-context APIs, nonblocking. true means queued, NOT completed. */
/** @brief 外部任务非阻塞提交命令；true 只代表入队，false 表示参数非法或队列满。
 * 不可从 ISR 调用。队列按值复制；外部命令被取出后退出自动自检。 */
bool MotorTask_Submit(const motor_command_t *cmd);
/** @brief 任务上下文发出独立停止请求；下一次 MotorTask 循环中清队列并中止自检/路径。
 * 不受命令队列满限制；这是软件串口停止，不能保证失联驱动器已停机。 */
void MotorTask_Stop(void); /* out-of-band, drops queued paths/commands */
/** @brief 在短临界区复制一致状态快照；任务上下文使用，out=NULL 时不操作。 */
void MotorTask_GetStatus(motor_task_status_t *out);
/** @brief 覆盖 CubeMX Weak 入口：初始化三路通信，然后持续推进通信、路径、自检和命令。
 * 等待事件时阻塞；不会在某个模式中另建无限循环，电机对象仅归本任务所有。 */
void StartMotorTask(void *argument);
/** @brief TIM2 ISR 累加节拍计数并唤醒 MotorTask；计数用于发现被合并的通知和漏节拍。 */
void MotorTask_TickFromISR(void);

/** @brief 提交固定动作；只表示入队，结果查看 action_status/action_stage。 */
bool MotorTask_RunAction(motor_action_mode_t mode);
