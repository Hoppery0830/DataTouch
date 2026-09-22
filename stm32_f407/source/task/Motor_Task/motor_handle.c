/**
 * @file motor_handle.c
 * @brief 当前路径点的非阻塞执行器；下发选中轴、确认到位、处理停留。
 * @see 本目录 README.md（职责、调用顺序与使用限制）。
 */
#include "motor_handle.h"
/** @brief 推进当前点一次：-1=故障或超时，0=等待，1=全部选中轴到位且停留完成。
 * 每轴只下发一次，不阻塞等待；段超时包含下发等待、运动和停留。 */
int Motor_Handle_Point(motion_state_t *s, motor_t motors[AXIS_COUNT], uint32_t now)
{
    const path_point_t *p = &s->path.points[s->point];
    bool reached = true;
    if (now - s->segment_ms >= s->path.segment_timeout_ms)
        return -1;
    for (unsigned i = 0; i < AXIS_COUNT; i++)
    {
        uint8_t bit = (uint8_t)(1U << i);
        if (!(s->path.axis_mask & bit))
            continue;
        motor_t *m = &motors[i];
        if (!Motor_Healthy(m, now) || !m->origin_valid || !m->enable_requested || !(m->flags & 1U))
            return -1;
        if (!(s->sent_mask & bit))
        {
            if (!m->pending && !m->port->tx_busy)
            {
                if (!Axis_Move(m, (axis_id_t)i, p->target[i], p->speed[i], now))
                    return -1;
                s->sent_mask |= bit;
                s->sent_ms[i] = now;
            }
            reached = false;
        }
        else if (!Axis_AtTarget(m, (axis_id_t)i, p->target[i], s->sent_ms[i], now))
            reached = false;
    }
    if (!reached)
    {
        s->dwelling = false;
        return 0;
    }
    if (!s->dwelling)
    {
        s->dwelling = true;
        s->dwell_start_ms = now;
    }
    if (now - s->dwell_start_ms < p->dwell_ms)
        return 0;
    for (unsigned i = 0; i < AXIS_COUNT; i++)
        if (s->path.axis_mask & (1U << i))
            motors[i].mode = MOTOR_HOLD;
    return 1;
}
