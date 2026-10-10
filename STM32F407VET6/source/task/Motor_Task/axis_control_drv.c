/**
 * @file axis_control_drv.c
 * @brief 轴物理量适配；X/Z 使用 mm，Yaw 使用度，与脉冲和 RPM 分别换算。
 * @see 本目录 README.md（职责、调用顺序与使用限制）。
 */
#include "axis_control_drv.h"
#include <math.h>
/** @brief 校验位置/速度并换算方向、脉冲幅值和整数 RPM，失败返回 false。
 * X/Z 输入 mm、mm/s，Yaw 输入度、度/s；四舍五入存在量化，不修改硬件。 */
bool Axis_Convert(const axis_config_t *c, float pos, float speed, bool *rev, uint32_t *pulses,
                  uint16_t *rpm)
{
    if (!c || !rev || !pulses || !rpm || !isfinite(pos) || !isfinite(speed) ||
        !isfinite(c->units_per_motor_rev) || c->units_per_motor_rev <= 0 || !c->pulses_per_rev ||
        pos < c->min_position || pos > c->max_position || speed <= 0 || speed > c->max_speed)
        return false;
    double n = fabs((double)pos) / c->units_per_motor_rev * c->pulses_per_rev;
    double v = (double)speed / c->units_per_motor_rev * 60.0;
    if (n > UINT32_MAX - 0.5 || v < 0.5 || v > 5000.0)
        return false;
    *pulses = (uint32_t)(n + 0.5);
    *rpm = (uint16_t)(v + 0.5);
    *rev = (pos < 0) != c->reversed;
    return true;
}
/** @brief 将有符号编码器原始位置换成轴单位；65536 刻度等于电机一圈，含方向和传动换算。 */
float Axis_Position(const axis_config_t *c, int64_t raw)
{
    return (float)((double)raw / 65536.0 * c->units_per_motor_rev) * (c->reversed ? -1.0f : 1.0f);
}
/** @brief 按轴配置校验物理目标并调用 Motor_Move；true 是下发成功，不是到位。 */
bool Axis_Move(motor_t *m, axis_id_t id, float pos, float speed, uint32_t now)
{
    bool rev;
    uint32_t pulses;
    uint16_t rpm;
    if (id >= AXIS_COUNT || !Axis_Convert(&g_axis_config[id], pos, speed, &rev, &pulses, &rpm))
        return false;
    return Motor_Move(m, rev, rpm, g_axis_config[id].acceleration, pulses, now);
}
/** @brief 同时检查健康、无未完成控制事务、两类反馈严格晚于 sent、到位标志与位置容差。
 * sent/now 均为毫秒；不能用下发前的旧位置或 ACK 代替运动完成。 */
bool Axis_AtTarget(const motor_t *m, axis_id_t id, float target, uint32_t sent, uint32_t now)
{
    /* 后台查询不打断到位停留；控制命令仍须完成，反馈新鲜度继续检查。 */
    bool query_only = !m->pending || m->pending == EMM_POSITION || m->pending == EMM_FLAGS ||
                      m->pending == EMM_SPEED;
    return Motor_Healthy(m, now) && query_only && (int32_t)(m->position_ms - sent) > 0 &&
           (int32_t)(m->flags_ms - sent) > 0 && (m->flags & 2U) &&
           fabsf(Axis_Position(&g_axis_config[id], m->position) - target) <=
               g_axis_config[id].tolerance;
}
