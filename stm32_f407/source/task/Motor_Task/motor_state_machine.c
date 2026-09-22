/**
 * @file motor_state_machine.c
 * @brief 运动路径模式与段切换；底层使能、速度/位置模式归 motor 模块。
 * @see 本目录 README.md（职责、调用顺序与使用限制）。
 */
#include "motor_state_machine.h"
#include "motor_handle.h"
#include <string.h>
/** @brief 预检整条路径的模式、选中轴、点数、循环数、时间和所有目标；非法路径不部分执行。 */
bool Motion_Validate(const motion_path_t *p)
{
    if (!p || p->mode > PATH_WAYPOINTS || p->mode < PATH_POINT_TO_POINT || !p->axis_mask ||
        (p->axis_mask & ~7U) || !p->cycles || !p->point_count || p->point_count > PATH_MAX_POINTS ||
        p->segment_timeout_ms < 100 || p->segment_timeout_ms > 3600000U)
        return false;
    if (p->mode == PATH_POINT_TO_POINT && (p->point_count != 1 || p->cycles != 1))
        return false;
    if (p->mode == PATH_RECIPROCATING && p->point_count != 2)
        return false;
    for (unsigned j = 0; j < p->point_count; j++)
    {
        if (p->points[j].dwell_ms >= p->segment_timeout_ms)
            return false;
        for (unsigned i = 0; i < AXIS_COUNT; i++)
            if (p->axis_mask & (1U << i))
            {
                bool rev;
                uint32_t pulses;
                uint16_t rpm;
                if (!Axis_Convert(&g_axis_config[i], p->points[j].target[i], p->points[j].speed[i],
                                  &rev, &pulses, &rpm))
                    return false;
            }
    }
    return true;
}
/** @brief 校验路径及选中轴状态，复制路径并进入运行态；true 表示接受路径，不是执行完毕。 */
bool Motion_Start(motion_state_t *s, const motion_path_t *p, motor_t motors[AXIS_COUNT],
                  uint32_t now)
{
    if (s->status == PATH_RUNNING || !Motion_Validate(p))
        return false;
    for (unsigned i = 0; i < AXIS_COUNT; i++)
        if (p->axis_mask & (1U << i))
        {
            motor_t *m = &motors[i];
            if (!Motor_Healthy(m, now) || !m->enable_requested || !m->origin_valid ||
                !(m->flags & 1U) || m->stop_requested)
                return false;
        }
    memset(s, 0, sizeof(*s));
    s->path = *p;
    s->status = PATH_RUNNING;
    s->segment_ms = now;
    return true;
}
/** @brief 将路径置为中止或失败，并请求三轴停止；串口应答/停止超时仍由 motor 模块处理。 */
void Motion_Abort(motion_state_t *s, motor_t motors[AXIS_COUNT], bool failed)
{
    s->status = failed ? PATH_FAILED : PATH_ABORTED;
    for (unsigned i = 0; i < AXIS_COUNT; i++)
        Motor_RequestStop(&motors[i]);
}
/** @brief 根据当前点执行结果切换点/轮次；每个控制节拍调用，now 为毫秒。
 * 所有选中轴到位且停留结束后才换点，失败向全部三轴请求停止。 */
void Motion_Update(motion_state_t *s, motor_t motors[AXIS_COUNT], uint32_t now)
{
    if (s->status != PATH_RUNNING)
        return;
    int result = Motor_Handle_Point(s, motors, now);
    if (result < 0)
    {
        Motion_Abort(s, motors, true);
        return;
    }
    if (!result)
        return;
    /* Point: one destination; reciprocating: A,B per cycle; waypoints: ordered list. */
    if (++s->point >= s->path.point_count)
    {
        s->point = 0;
        if (++s->cycle >= s->path.cycles)
        {
            s->status = PATH_DONE;
            return;
        }
    }
    s->sent_mask = 0;
    s->dwelling = false;
    s->segment_ms = now;
}
