/** @file motor_startup.h
 * @brief 上电单次固定动作生成器；仅由 MotorTask 调用，详见 STARTUP.md。
 */
#pragma once
#include "motor_task.h"
typedef struct
{
    motor_startup_state_t state;
    uint32_t since, accepted_ms;
    uint8_t axis_mask, axis;
    bool dispatched, prepare_only;
    motor_action_mode_t mode;
} motor_startup_t;
void Startup_Init(motor_startup_t *s, uint32_t now);
/** 取消后锁存，不发停止命令；任务负责停止正在运行的路径。 */
void Startup_Cancel(motor_startup_t *s);
/** 接受只代表分发成功，各准备阶段仍等待新鲜反馈。 */
void Startup_CommandResult(motor_startup_t *s, bool accepted, uint32_t now);
/** 非阻塞生成一条命令；分发结果返回前不得再次索取命令。 */
bool Startup_Update(motor_startup_t *s, const motor_t motors[AXIS_COUNT],
                    const motor_action_t *action, uint32_t now, motor_command_t *out);

void Startup_Prepare(motor_startup_t *s, motor_action_mode_t mode, uint32_t now);
