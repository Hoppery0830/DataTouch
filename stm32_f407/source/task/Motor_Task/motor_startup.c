/** @file motor_startup.c
 * @brief 等待参与轴在线，逐轴使能/设零，执行一次选定动作。详见 STARTUP.md。
 */
#include "motor_startup.h"
#include <math.h>
#include <string.h>
_Static_assert(APP_AUTOSTART_MODE >= ACTION_PRESS && APP_AUTOSTART_MODE < ACTION_MODE_COUNT,
               "Invalid startup action");
static bool active(const motor_startup_t *s)
{
    return s->state >= STARTUP_WAIT_ONLINE && s->state <= STARTUP_ACTION;
}
static void enter(motor_startup_t *s, motor_startup_state_t state, uint32_t now)
{
    s->state = state;
    s->since = now;
    s->dispatched = false;
}
void Startup_Init(motor_startup_t *s, uint32_t now)
{
    memset(s, 0, sizeof(*s));
    s->mode = APP_AUTOSTART_MODE;
    s->axis_mask = (1U << MOTOR_Z) | (APP_AUTOSTART_MODE == ACTION_RUB     ? (1U << MOTOR_YAW)
                                      : APP_AUTOSTART_MODE == ACTION_SLIDE ? (1U << MOTOR_X)
                                                                           : 0);
    while (!(s->axis_mask & (1U << s->axis)))
        s->axis++;
    enter(s, APP_MOTOR_AUTOSTART ? STARTUP_WAIT_ONLINE : STARTUP_DISABLED, now);
}
void Startup_Prepare(motor_startup_t *s, motor_action_mode_t mode, uint32_t now)
{
    memset(s, 0, sizeof(*s));
    s->mode = mode;
    s->prepare_only = true;
    s->axis_mask = (1U << MOTOR_Z) | (mode == ACTION_RUB     ? (1U << MOTOR_YAW)
                                      : mode == ACTION_SLIDE ? (1U << MOTOR_X)
                                                             : 0);
    while (!(s->axis_mask & (1U << s->axis)))
        s->axis++;
    enter(s, STARTUP_WAIT_ONLINE, now);
}
void Startup_Cancel(motor_startup_t *s)
{
    if (active(s))
        s->state = STARTUP_STOPPED;
}
void Startup_CommandResult(motor_startup_t *s, bool accepted, uint32_t now)
{
    if (!active(s))
        return;
    if (!accepted)
    {
        s->state = STARTUP_FAILED;
        return;
    }
    s->dispatched = true;
    s->accepted_ms = now;
    s->since = now;
}
bool Startup_Update(motor_startup_t *s, const motor_t motors[AXIS_COUNT],
                    const motor_action_t *action, uint32_t now, motor_command_t *out)
{
    if (!active(s))
        return false;
    /* 动作运行超时由每段路径管理，准备阶段各自限时。 */
    if (s->state != STARTUP_ACTION && now - s->since >= APP_AUTOSTART_SETUP_MS)
    {
        s->state = STARTUP_FAILED;
        return false;
    }
    bool all_online = true;
    for (unsigned i = 0; i < AXIS_COUNT; i++)
        if (s->axis_mask & (1U << i))
        {
            const motor_t *m = &motors[i];
            if (m->fault || m->port->broken || m->stop_requested || m->stop_failed)
            {
                s->state = STARTUP_FAILED;
                return false;
            }
            bool healthy = Motor_Healthy(m, now);
            all_online &= healthy;
            /* 当前清零轴允许位置短暂无效，其他已在线轴不能失联。 */
            if (!healthy && s->state != STARTUP_WAIT_ONLINE &&
                !(s->state == STARTUP_ZERO && i == s->axis))
            {
                s->state = STARTUP_FAILED;
                return false;
            }
        }
    const motor_t *m = &motors[s->axis];
    memset(out, 0, sizeof(*out));
    out->axis = (axis_id_t)s->axis;
    switch (s->state)
    {
    case STARTUP_WAIT_ONLINE:
        if (all_online)
            enter(s, STARTUP_ENABLE, now);
        break;
    case STARTUP_ENABLE:
        if (!s->dispatched)
        {
            out->kind = MOTOR_CMD_ENABLE;
            out->enable = true;
            return true;
        }
        if (!m->pending && m->enable_requested && (m->flags & 1U) &&
            (int32_t)(m->flags_ms - s->accepted_ms) > 0)
            enter(s, STARTUP_ZERO, now);
        break;
    case STARTUP_ZERO:
        if (!s->dispatched && !(s->prepare_only && m->origin_valid))
        {
            out->kind = MOTOR_CMD_ORIGIN;
            return true;
        }
        if ((s->prepare_only && !s->dispatched && m->origin_valid && m->enable_requested &&
             (m->flags & 1U) && Motor_Healthy(m, now) && !m->pending) ||
            (Motor_Healthy(m, now) && !m->pending && m->origin_valid && m->enable_requested &&
             (m->flags & 1U) && (int32_t)(m->position_ms - s->accepted_ms) > 0 &&
             (int32_t)(m->flags_ms - s->accepted_ms) > 0 &&
             fabsf(Axis_Position(&g_axis_config[s->axis], m->position)) <=
                 g_axis_config[s->axis].tolerance))
        {
            unsigned next = s->axis + 1;
            while (next < AXIS_COUNT && !(s->axis_mask & (1U << next)))
                next++;
            if (next < AXIS_COUNT)
            {
                s->axis = (uint8_t)next;
                enter(s, STARTUP_ENABLE, now);
            }
            else
                enter(s, s->prepare_only ? STARTUP_DONE : STARTUP_ACTION, now);
        }
        break;
    case STARTUP_ACTION:
        if (!s->dispatched)
        {
            out->kind = MOTOR_CMD_ACTION;
            out->action_mode = s->mode;
            return true;
        }
        if (action->status == ACTION_DONE)
            s->state = STARTUP_DONE;
        else if (action->status == ACTION_FAILED || action->status == ACTION_ABORTED)
            s->state = STARTUP_FAILED;
        break;
    default:
        break;
    }
    return false;
}
