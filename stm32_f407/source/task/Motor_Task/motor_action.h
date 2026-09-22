/** @file motor_action.h
 * @brief 按压/揉搓/滑动的固定动作状态机；复用 Motion 路径执行器。
 * @see ACTIONS.md。所有接口由 MotorTask 独占调用。
 */
#pragma once
#include "motor_state_machine.h"
typedef enum
{
    ACTION_IDLE = 0,
    ACTION_RUNNING,
    ACTION_DONE,
    ACTION_ABORTED,
    ACTION_FAILED
} motor_action_status_t;
typedef enum
{
    ACTION_STAGE_NONE = 0,
    ACTION_STAGE_PRESS,
    ACTION_STAGE_RUB,
    ACTION_STAGE_YAW_RETURN,
    ACTION_STAGE_SLIDE,
    ACTION_STAGE_RETRACT,
    ACTION_STAGE_X_RETURN,
    ACTION_STAGE_FINISHED
} motor_action_stage_t;
typedef struct
{
    motor_action_mode_t mode;
    motor_action_status_t status;
    motor_action_stage_t stage;
    float start[AXIS_COUNT]; /**< 动作接受时的位置快照；X/Z mm，Yaw 度。 */
    uint8_t axis_mask;
} motor_action_t;
/** @brief 校验全部阶段和参与轴后接受动作；false 时不启动任何阶段，不修改原动作。 */
bool Action_Start(motor_action_t *a, motor_action_mode_t mode, motion_state_t *motion,
                  motor_t motors[AXIS_COUNT], uint32_t now);
/** @brief 在 Motion_Update 之后调用；完成当前阶段后进入下一阶段，失败停止三轴。 */
void Action_Update(motor_action_t *a, motion_state_t *motion, motor_t motors[AXIS_COUNT],
                   uint32_t now);
/** @brief 标记活动动作取消；实际停止由调用者执行 Motion_Abort，不自动退回。 */
void Action_Cancel(motor_action_t *a);
/** @brief Handle：生成某一阶段的有限路径；用于全动作预检和实际下发。 */
bool Action_BuildPath(const motor_action_t *a, motor_action_stage_t stage, motion_path_t *out);
