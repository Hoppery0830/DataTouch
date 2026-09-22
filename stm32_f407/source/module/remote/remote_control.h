/** @brief 由 MotorTask 独占的远程会话：准备、执行、停止和结果缓存。 */
#pragma once
#include "remote_protocol.h"
#include "motor_startup.h"
typedef void (*remote_emit_t)(remote_reply_t reply);
typedef struct
{
    remote_status_t status;
    motor_startup_t preparation;
    uint32_t active_seq, stop_ms;
    uint8_t mask, stop_target;
    bool selected, active;
    struct
    {
        remote_request_t request;
        uint8_t result;
        bool valid;
    } cache[16];
    unsigned next;
    remote_emit_t emit;
} remote_control_t;
void Remote_Init(remote_control_t *r, remote_emit_t emit);
void Remote_Handle(remote_control_t *r, remote_request_t req, motor_t m[AXIS_COUNT],
                   motion_state_t *path, motor_action_t *action, uint32_t now);
void Remote_Update(remote_control_t *r, motor_t m[AXIS_COUNT], motion_state_t *path,
                   motor_action_t *action, uint32_t now, bool link_alive);
/** 本地停止同样终止远程会话，不继续准备。 */
void Remote_Cancel(remote_control_t *r, motor_t m[AXIS_COUNT], motion_state_t *path,
                   motor_action_t *action, uint32_t now);
