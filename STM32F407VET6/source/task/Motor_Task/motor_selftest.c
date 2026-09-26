/**
 * @file motor_selftest.c
 * @brief 上电自检分步状态机；由正常 MotorTask 循环调用，不建立额外任务。
 * @see 本目录 README.md（职责、调用顺序与使用限制）。
 */
#include "motor_selftest.h"
#include <math.h>
#include <string.h>
#define SETUP_TIMEOUT_MS 5000U
#define SEGMENT_TIMEOUT_MS 10000U
#define REPEAT_DELAY_MS 500U
_Static_assert(APP_SELFTEST_AXIS >= MOTOR_X && APP_SELFTEST_AXIS < AXIS_COUNT,
               "Invalid selftest axis");
/** @brief 切换自检步骤，重置等待起点和命令已下发标记。 */
static void enter(motor_selftest_t *s, selftest_state_t state, uint32_t now)
{
    s->state = state;
    s->since = now;
    s->dispatched = false;
}
/** @brief 按编译开关初始化自检；记录起始毫秒时刻，不直接发送命令。 */
void SelfTest_Init(motor_selftest_t *s, uint32_t now)
{
    memset(s, 0, sizeof(*s));
    enter(s, APP_MOTOR_SELFTEST ? SELFTEST_WAIT_ONLINE : SELFTEST_DISABLED, now);
}
/** @brief 取消自检并锁存在 STOPPED；DISABLED/FAILED 保持原值，本函数本身不发停止命令。 */
void SelfTest_Cancel(motor_selftest_t *s)
{
    if (s->state != SELFTEST_DISABLED && s->state != SELFTEST_FAILED)
        s->state = SELFTEST_STOPPED;
}
/** @brief 接收正常分发器的下发结果；accepted=true 不是电机 ACK，仍需后续反馈确认。
 * 失败锁存 FAILED；终止态忽略结果。必须与前一步生成的自检命令对应。 */
void SelfTest_CommandResult(motor_selftest_t *s, bool accepted, uint32_t now)
{
    if (s->state == SELFTEST_DISABLED || s->state == SELFTEST_STOPPED ||
        s->state == SELFTEST_FAILED)
        return;
    if (!accepted)
    {
        s->state = SELFTEST_FAILED;
        return;
    }
    s->dispatched = true;
    s->accepted_ms = now;
    s->since = now;
}
/** @brief 执行一次自检步骤；true 时 out 产生一条待分发命令，false 时等待或已终止。
 * 调用者在命令处理完之前不重复索取命令，随后调用 CommandResult；now 单位毫秒。 */
bool SelfTest_Update(motor_selftest_t *s, const motor_t motors[AXIS_COUNT],
                     const motion_state_t *motion, uint32_t now, motor_command_t *out)
{
    if (s->state == SELFTEST_DISABLED || s->state == SELFTEST_STOPPED ||
        s->state == SELFTEST_FAILED)
        return false;
    const motor_t *m = &motors[APP_SELFTEST_AXIS];
    if (m->fault || m->port->broken || m->stop_requested || m->stop_failed)
    {
        s->state = SELFTEST_FAILED;
        return false;
    }
    uint32_t limit =
        s->state == SELFTEST_PATH ? 2U * SEGMENT_TIMEOUT_MS + SETUP_TIMEOUT_MS : SETUP_TIMEOUT_MS;
    if (now - s->since >= limit)
    {
        s->state = SELFTEST_FAILED;
        return false;
    }
    bool healthy = Motor_Healthy(m, now);
    /* ZERO temporarily invalidates position until a fresh query returns. */
    if (s->state != SELFTEST_WAIT_ONLINE && s->state != SELFTEST_ZERO && !healthy)
    {
        s->state = SELFTEST_FAILED;
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->axis = APP_SELFTEST_AXIS;
    switch (s->state)
    {
    case SELFTEST_WAIT_ONLINE:
        if (healthy)
            enter(s, SELFTEST_ENABLE, now);
        break;
    /* 已下发仍要等更新的状态回读，防止沿用旧的 enabled 标志。 */
    case SELFTEST_ENABLE:
        if (!s->dispatched)
        {
            out->kind = MOTOR_CMD_ENABLE;
            out->enable = true;
            return true;
        }
        if (!m->pending && m->enable_requested && (m->flags & 1U) &&
            (int32_t)(m->flags_ms - s->accepted_ms) > 0)
            enter(s, SELFTEST_ZERO, now);
        break;
    /* 清零 ACK 会暂时使位置无效：给轮询留时间，直到新位置接近零。 */
    case SELFTEST_ZERO:
        if (!s->dispatched)
        {
            out->kind = MOTOR_CMD_ORIGIN;
            return true;
        }
        if (healthy && !m->pending && m->origin_valid && m->enable_requested && (m->flags & 1U) &&
            (int32_t)(m->position_ms - s->accepted_ms) > 0 &&
            (int32_t)(m->flags_ms - s->accepted_ms) > 0 &&
            fabsf(Axis_Position(&g_axis_config[APP_SELFTEST_AXIS], m->position)) <=
                g_axis_config[APP_SELFTEST_AXIS].tolerance)
            enter(s, SELFTEST_PATH, now);
        break;
    case SELFTEST_PATH:
        if (!s->dispatched)
        {
            out->kind = MOTOR_CMD_PATH;
            out->path.mode = PATH_RECIPROCATING;
            out->path.axis_mask = (uint8_t)(1U << APP_SELFTEST_AXIS);
            out->path.point_count = 2;
            out->path.cycles = 1;
            out->path.segment_timeout_ms = SEGMENT_TIMEOUT_MS;
            /* One round goes target -> zero. Completed paths may be repeated. */
            out->path.points[0].target[APP_SELFTEST_AXIS] = APP_SELFTEST_TARGET;
            out->path.points[0].speed[APP_SELFTEST_AXIS] = APP_SELFTEST_SPEED;
            out->path.points[1].target[APP_SELFTEST_AXIS] = 0;
            out->path.points[1].speed[APP_SELFTEST_AXIS] = APP_SELFTEST_SPEED;
            return true;
        }
        if (motion->status == PATH_FAILED || motion->status == PATH_ABORTED)
        {
            s->state = SELFTEST_FAILED;
            return false;
        }
        if (motion->status == PATH_DONE)
        {
            s->rounds++;
            enter(s, SELFTEST_PAUSE, now);
        }
        break;
    /* 用时间差代替 osDelay；等待间隔期间 MotorTask 仍处理串口及停止。 */
    case SELFTEST_PAUSE:
        if (now - s->since >= REPEAT_DELAY_MS)
            enter(s, SELFTEST_PATH, now);
        break;
    default:
        break;
    }
    return false;
}
