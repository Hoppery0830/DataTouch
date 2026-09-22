/**
 * @file axis_control_drv.h
 * @brief 轴物理量适配；X/Z 使用 mm，Yaw 使用度，与脉冲和 RPM 分别换算。
 * @see 本目录 README.md（职责、调用顺序与使用限制）。
 */
#pragma once
#include "motor_config.h"
#include "motor.h"
/** @brief 校验位置/速度并换算方向、脉冲幅值和整数 RPM，失败返回 false。
 * X/Z 输入 mm、mm/s，Yaw 输入度、度/s；四舍五入存在量化，不修改硬件。 */
bool Axis_Convert(const axis_config_t *c, float position, float speed, bool *reverse,
                  uint32_t *pulses, uint16_t *rpm);
/** @brief 将有符号编码器原始位置换成轴单位；65536 刻度等于电机一圈，含方向和传动换算。 */
float Axis_Position(const axis_config_t *c, int64_t raw);
/** @brief 按轴配置校验物理目标并调用 Motor_Move；true 是下发成功，不是到位。 */
bool Axis_Move(motor_t *m, axis_id_t axis, float position, float speed, uint32_t now);
/** @brief 同时检查健康、无未完成事务、两类反馈严格晚于 sent、到位标志与位置容差。
 * sent/now 均为毫秒；不能用下发前的旧位置或 ACK 代替运动完成。 */
bool Axis_AtTarget(const motor_t *m, axis_id_t axis, float target, uint32_t sent_ms, uint32_t now);
