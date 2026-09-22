/**
 * @file motor_selftest.h
 * @brief 上电自检分步状态机；由正常 MotorTask 循环调用，不建立额外任务。
 * @see 本目录 README.md（职责、调用顺序与使用限制）。
 */
#pragma once
#include "motor_task.h"
typedef struct
{
    selftest_state_t state;
    uint32_t since, accepted_ms, rounds;
    bool dispatched;
} motor_selftest_t;
/** @brief 按编译开关初始化自检；记录起始毫秒时刻，不直接发送命令。 */
void SelfTest_Init(motor_selftest_t *s, uint32_t now);
/** @brief 取消自检并锁存在 STOPPED；DISABLED/FAILED 保持原值，本函数本身不发停止命令。 */
void SelfTest_Cancel(motor_selftest_t *s);
/* One bounded step. Returns one command for the normal MotorTask dispatcher. */
/** @brief 执行一次自检步骤；true 时 out 产生一条待分发命令，false 时等待或已终止。
 * 调用者在命令处理完之前不重复索取命令，随后调用 CommandResult；now 单位毫秒。 */
bool SelfTest_Update(motor_selftest_t *s, const motor_t motors[AXIS_COUNT],
                     const motion_state_t *motion, uint32_t now, motor_command_t *out);
/** @brief 接收正常分发器的下发结果；accepted=true 不是电机 ACK，仍需后续反馈确认。
 * 失败锁存 FAILED；终止态忽略结果。必须与前一步生成的自检命令对应。 */
void SelfTest_CommandResult(motor_selftest_t *s, bool accepted, uint32_t now);
