/**
 * @file remote_control.h
 * @brief 由 MotorTask 管理远程会话、采图门控、运动和结果缓存。
 */

#pragma once
#include "remote_protocol.h"
#include "motor_startup.h"
#include "capture_sequence.h"
/// @brief 远程 START 在运行路径前使用的停稳与采图接口。
typedef struct
{
    bool (*stationary)(const motor_t motors[AXIS_COUNT], uint32_t now);
    bool (*begin)(uint32_t seq, uint32_t now);
    void (*status)(uint32_t seq, capture_sequence_t *out);
    void (*cancel)(void);
    bool (*light_off)(void);
    bool (*available)(void); /**< 上轮结果交接完成，允许开启下一轮采图。 */
} remote_vision_ops_t;
typedef void (*remote_emit_t)(remote_reply_t reply);
typedef struct
{
    remote_status_t status;
    const remote_vision_ops_t *vision;
    uint32_t capture_wait_ms;
    float capture_position[AXIS_COUNT];
    bool capture_started;
    motor_startup_t preparation;
    uint32_t active_seq, stop_ms;
    uint8_t mask, stop_target;
    bool selected, active;
    bool resetting; /**< RESET 正在等待三轴停止确认与稳定反馈。 */
    uint8_t reset_stable_mask, reset_clear_mask;
    uint32_t reset_acks[AXIS_COUNT], reset_retry[AXIS_COUNT], reset_stable[AXIS_COUNT];
    struct
    {
        remote_request_t request;
        uint8_t result;
        bool valid;
    } cache[16];
    unsigned next;
    remote_emit_t emit;
} remote_control_t;
/**
 * @brief 初始化 MotorTask 独占的远程控制会话。
 * @param r 要初始化的会话。
 * @param emit 命令结果回调。
 */
void Remote_Init(remote_control_t *r, remote_emit_t emit);
/**
 * @brief 处理远程命令并按序号复用已缓存的结果。
 * @param r 远程控制会话。
 * @param req 本次远程请求。
 * @param m 三轴电机状态。
 * @param path 运动路径状态。
 * @param action 试验动作状态。
 * @param now 当前 HAL 毫秒计数。
 */
void Remote_Handle(remote_control_t *r, remote_request_t req, motor_t m[AXIS_COUNT],
                   motion_state_t *path, motor_action_t *action, uint32_t now);
/**
 * @brief 推进准备、停稳检查、采图门控、运动和停止。
 * @param r 远程控制会话。
 * @param m 三轴电机状态。
 * @param path 运动路径状态。
 * @param action 试验动作状态。
 * @param now 当前 HAL 毫秒计数。
 * @param link_alive ESP32 链路心跳是否仍有效。
 */
void Remote_Update(remote_control_t *r, motor_t m[AXIS_COUNT], motion_state_t *path,
                   motor_action_t *action, uint32_t now, bool link_alive);

/**
 * @brief 终止远程会话并请求停止。
 * @param r 远程控制会话。
 * @param m 三轴电机状态。
 * @param path 运动路径状态。
 * @param action 试验动作状态。
 * @param now 当前 HAL 毫秒计数。
 * @note 本地停止也走此入口；取消后不会继续准备或采图。
 */
void Remote_Cancel(remote_control_t *r, motor_t m[AXIS_COUNT], motion_state_t *path,
                   motor_action_t *action, uint32_t now);

/**
 * @brief 为远程会话绑定采图联动回调。
 * @param r 远程控制会话。
 * @param ops 停稳判断和采图接口；使用期间须保持有效。
 */
void Remote_SetVision(remote_control_t *r, const remote_vision_ops_t *ops);
