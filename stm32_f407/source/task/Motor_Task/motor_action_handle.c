/** @file motor_action_handle.c
 * @brief 固定动作路径构造；不访问硬件、不改变电机状态。
 */
#include "motor_action.h"
#include <string.h>
#include <math.h>
/** @brief 构造绝对目标；所有返回位置均使用动作开始时快照，不清零坐标。 */
bool Action_BuildPath(const motor_action_t *a, motor_action_stage_t stage, motion_path_t *p)
{
    if (!a || !p || (unsigned)a->mode >= ACTION_MODE_COUNT)
        return false;
    const motor_action_config_t *c = &g_action_config[a->mode];
    if (!isfinite(c->z_press_mm) || c->z_press_mm == 0)
        return false;
    memset(p, 0, sizeof(*p));
    p->mode = PATH_POINT_TO_POINT;
    p->point_count = 1;
    p->cycles = 1;
    p->segment_timeout_ms = c->segment_timeout_ms;
    axis_id_t axis = MOTOR_Z;
    float target = a->start[MOTOR_Z], speed = c->z_speed_mm_s;
    switch (stage)
    {
    case ACTION_STAGE_PRESS:
        target += c->z_press_mm;
        p->points[0].dwell_ms = c->hold_ms;
        break;
    case ACTION_STAGE_RETRACT:
        break;
    case ACTION_STAGE_RUB:
        if (a->mode != ACTION_RUB || !isfinite(c->yaw_amplitude_deg) || c->yaw_amplitude_deg <= 0 ||
            !c->rub_cycles)
            return false;
        axis = MOTOR_YAW;
        speed = c->yaw_speed_deg_s;
        target = a->start[axis] - c->yaw_amplitude_deg;
        p->mode = PATH_RECIPROCATING;
        p->point_count = 2;
        p->cycles = c->rub_cycles;
        p->points[1].target[axis] = a->start[axis] + c->yaw_amplitude_deg;
        p->points[1].speed[axis] = speed;
        break;
    case ACTION_STAGE_YAW_RETURN:
        if (a->mode != ACTION_RUB)
            return false;
        axis = MOTOR_YAW;
        target = a->start[axis];
        speed = c->yaw_speed_deg_s;
        break;
    case ACTION_STAGE_SLIDE:
    case ACTION_STAGE_X_RETURN:
        if (a->mode != ACTION_SLIDE || !isfinite(c->x_distance_mm) || c->x_distance_mm == 0)
            return false;
        axis = MOTOR_X;
        target = a->start[axis];
        speed = c->x_speed_mm_s;
        if (stage == ACTION_STAGE_SLIDE)
            target += c->x_distance_mm;
        break;
    default:
        return false;
    }
    p->axis_mask = (uint8_t)(1U << axis);
    p->points[0].target[axis] = target;
    p->points[0].speed[axis] = speed;
    return Motion_Validate(p);
}
