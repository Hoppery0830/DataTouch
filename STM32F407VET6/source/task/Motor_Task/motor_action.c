/** @file motor_action.c
 * @brief 固定动作阶段管理：按压、旋转揉搓、接触滑动。
 * @see ACTIONS.md。使用位置保持，无力控和轨迹插补。
 */
#include "motor_action.h"
#include <string.h>
static motor_action_stage_t next_stage(motor_action_mode_t mode, motor_action_stage_t stage)
{
    switch (stage)
    {
    case ACTION_STAGE_PRESS:
        return mode == ACTION_RUB     ? ACTION_STAGE_RUB
               : mode == ACTION_SLIDE ? ACTION_STAGE_SLIDE
                                      : ACTION_STAGE_RETRACT;
    case ACTION_STAGE_RUB:
        return ACTION_STAGE_YAW_RETURN;
    case ACTION_STAGE_YAW_RETURN:
    case ACTION_STAGE_SLIDE:
        return ACTION_STAGE_RETRACT;
    case ACTION_STAGE_RETRACT:
        return mode == ACTION_SLIDE ? ACTION_STAGE_X_RETURN : ACTION_STAGE_FINISHED;
    case ACTION_STAGE_X_RETURN:
        return ACTION_STAGE_FINISHED;
    default:
        return ACTION_STAGE_FINISHED;
    }
}
static bool ready(const motor_action_t *a, motor_t motors[AXIS_COUNT], uint32_t now)
{
    for (unsigned i = 0; i < AXIS_COUNT; i++)
        if (a->axis_mask & (1U << i))
        {
            motor_t *m = &motors[i];
            if (!Motor_Healthy(m, now) || !m->origin_valid || !m->enable_requested ||
                !(m->flags & 1U) || m->stop_requested || m->stop_failed)
                return false;
        }
    return true;
}
bool Action_Start(motor_action_t *a, motor_action_mode_t mode, motion_state_t *motion,
                  motor_t motors[AXIS_COUNT], uint32_t now)
{
    if (!a || !motion || !motors || (unsigned)mode >= ACTION_MODE_COUNT ||
        a->status == ACTION_RUNNING || motion->status == PATH_RUNNING)
        return false;
    motor_action_t candidate = {0};
    candidate.mode = mode;
    candidate.axis_mask = (uint8_t)((1U << MOTOR_Z) | (mode == ACTION_RUB     ? (1U << MOTOR_YAW)
                                                       : mode == ACTION_SLIDE ? (1U << MOTOR_X)
                                                                              : 0));
    if (!ready(&candidate, motors, now))
        return false;
    for (unsigned i = 0; i < AXIS_COUNT; i++)
        if (candidate.axis_mask & (1U << i))
        {
            motor_t *m = &motors[i];
            /* 允许后台查询；拒绝仍在执行/确认的运动、使能或清零，防止记录漂移起点。 */
            if (m->mode == MOTOR_POSITION_MODE || m->mode == MOTOR_SPEED_MODE ||
                (m->pending && m->pending != EMM_POSITION && m->pending != EMM_FLAGS &&
                 m->pending != EMM_SPEED))
                return false;
            candidate.start[i] = Axis_Position(&g_axis_config[i], m->position);
        }
    motion_path_t path;
    /* 先预检压入、偏转/滑动和全部返回点；后段越界也不能先压下去。 */
    for (motor_action_stage_t st = ACTION_STAGE_PRESS; st != ACTION_STAGE_FINISHED;
         st = next_stage(mode, st))
        if (!Action_BuildPath(&candidate, st, &path))
            return false;
    if (!Action_BuildPath(&candidate, ACTION_STAGE_PRESS, &path) ||
        !Motion_Start(motion, &path, motors, now))
        return false;
    candidate.status = ACTION_RUNNING;
    candidate.stage = ACTION_STAGE_PRESS;
    *a = candidate;
    return true;
}
void Action_Cancel(motor_action_t *a)
{
    if (a && a->status == ACTION_RUNNING)
        a->status = ACTION_ABORTED;
}
void Action_Update(motor_action_t *a, motion_state_t *motion, motor_t motors[AXIS_COUNT],
                   uint32_t now)
{
    if (a->status != ACTION_RUNNING)
        return;
    if (motion->status == PATH_ABORTED)
    {
        Action_Cancel(a);
        return;
    }
    /* 各阶段只移动指定轴，但对整个动作参与轴持续检查健康。 */
    if (!ready(a, motors, now) || motion->status == PATH_FAILED)
    {
        a->status = ACTION_FAILED;
        Motion_Abort(motion, motors, true);
        return;
    }
    if (motion->status != PATH_DONE)
        return;
    motor_action_stage_t next = next_stage(a->mode, a->stage);
    if (next == ACTION_STAGE_FINISHED)
    {
        a->stage = next;
        a->status = ACTION_DONE;
        return;
    }
    motion_path_t path;
    if (!Action_BuildPath(a, next, &path) || !Motion_Start(motion, &path, motors, now))
    {
        a->status = ACTION_FAILED;
        Motion_Abort(motion, motors, true);
        return;
    }
    a->stage = next;
}
