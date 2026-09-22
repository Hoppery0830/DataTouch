/**
 * @file motor.h
 * @brief 单电机事务与反馈管理；状态只由 MotorTask 修改，不包含运动路径。
 * @see 本目录 README.md（职责、调用顺序与使用限制）。
 */
#pragma once
#include "uart_bsp.h"
#include "emm_protocol.h"
typedef enum
{
    MOTOR_NO_FAULT = 0,
    MOTOR_COMM_FAULT,
    MOTOR_REJECTED,
    MOTOR_STALL
} motor_fault_t;
typedef enum
{
    MOTOR_DISABLED = 0,
    MOTOR_POSITION_MODE,
    MOTOR_SPEED_MODE,
    MOTOR_HOLD
} motor_mode_t;
typedef struct
{
    uart_port_t *port;
    emm_parser_t parser;
    uint8_t address, pending, flags; /**< 地址、待应答功能码（0=空闲）、驱动器状态位。 */
    bool enable_requested, pending_enable, origin_valid, position_valid, flags_valid,
        stop_requested;
    bool stop_failed; /**< 停止被拒绝或超时；必须再取得停止成功应答才能清故障。 */
    motor_fault_t fault;
    motor_mode_t mode;
    int64_t position; /**< 有符号编码器刻度，65536/电机转；不是位置命令脉冲数。 */
    int32_t rpm;      /**< 最近速度反馈，带方向符号；位置/状态另有新鲜度检查。 */
    uint32_t position_ms, flags_ms, pending_ms, last_rx_ms, next_poll_ms, byte_ms, recovery_ms;
    uint32_t completed, timeouts, rejected; /**< 事务应答处理、超时、拒绝次数；不是运动完成次数。 */
    uint8_t poll_phase;
} motor_t;
/* All functions except external task queue wrappers are MotorTask-owned. */
/** @brief 初始化单电机状态并安排首次查询；不发送使能/运动命令。now 单位为 HAL 毫秒。 */
void Motor_Init(motor_t *m, uart_port_t *port, uint8_t address, uint32_t now);
/** @brief 处理有限数量接收字节、匹配应答、检查超时、恢复端口并优先推进停止。
 * 每轮 MotorTask 调用，now 为毫秒；不会自动重发不确定是否已执行的运动命令。 */
void Motor_Process(motor_t *m, uint32_t now);
/** @brief 判断无锁存故障且位置/状态反馈均在有效期内；不等同于已使能、已清零或已到位。 */
bool Motor_Healthy(const motor_t *m, uint32_t now);
/** @brief 请求使能或失能；true 表示开始等待应答，最终以 ACK 和状态回读为准。
 * 忙、停止中或健康检查失败时返回 false，不在内部排队。 */
bool Motor_Enable(motor_t *m, bool enable, uint32_t now);
/** @brief 请求当前位置清零；只允许非运动模式且反馈健康。原点先标无效，ACK 后等待新位置。
 * 仅限任务上下文；false 表示未下发。调用者须确认轴位于合适的静止参考位置。 */
bool Motor_Zero(motor_t *m, uint32_t now);
/** @brief 下发绝对位置命令；必须已使能、原点有效且反馈健康。pulses 为幅值，rpm 为转/分钟。
 * true 仅表示下发成功；到位由路径层结合新位置、状态与容差判定。 */
bool Motor_Move(motor_t *m, bool reverse, uint16_t rpm, uint8_t acc, uint32_t pulses, uint32_t now);
/** @brief 下发速度模式命令，方向和 RPM 已由上层决定；返回是否开始事务。
 * 此接口本身不执行物理行程约束，上层必须负责持续速度运动的边界与停止。 */
bool Motor_Velocity(motor_t *m, bool reverse, uint16_t rpm, uint8_t acc, uint32_t now);
/** @brief 任务侧置停止请求；Motor_Process 在发送缓冲可用时优先发停止，不在这里等待 ACK。 */
void Motor_RequestStop(motor_t *m);
/** @brief 满足通信恢复、反馈新鲜、无驱动器异常及无未完成停止时清除锁存故障。
 * 返回是否成功；不会重新建立原点，也不会自动恢复旧路径。 */
bool Motor_ClearFault(motor_t *m, uint32_t now);
/** @brief 在业务命令分发之后轮询位置、状态、速度；忙时跳过，避免查询长期抢占运动命令。 */
void Motor_Poll(motor_t *m, uint32_t now);
