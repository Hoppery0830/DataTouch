/**
 * @file motor.c
 * @brief 单电机事务与反馈管理；状态只由 MotorTask 修改，不包含运动路径。
 * @see 本目录 README.md（职责、调用顺序与使用限制）。
 */
#include "motor.h"
#include <string.h>
/** @brief 使用有符号差值判断毫秒截止时间；适用于间隔小于 2^31ms 的回绕比较。 */
static bool due(uint32_t now, uint32_t at)
{
    return (int32_t)(now - at) >= 0;
}
/** @brief 单电机事务入口：无待应答事务且 DMA 接受发送后，记录功能码和起始毫秒时刻。 */
static bool send(motor_t *m, uint8_t *b, size_t n, uint32_t now)
{
    if (m->pending || !n || !UART_BSP_Send(m->port, b, n))
        return false;
    m->pending = b[1];
    m->pending_ms = now;
    return true;
}
/** @brief 锁存首个故障并请求停止；使原点失效，通信恢复不自动清除此状态。 */
static void fault(motor_t *m, motor_fault_t f)
{
    if (m->fault == MOTOR_NO_FAULT)
    {
        m->fault = f;
        m->stop_requested = true;
    }
    m->origin_valid = false;
}
/** @brief 初始化单电机状态并安排首次查询；不发送使能/运动命令。now 单位为 HAL 毫秒。 */
void Motor_Init(motor_t *m, uart_port_t *p, uint8_t addr, uint32_t now)
{
    memset(m, 0, sizeof(*m));
    m->port = p;
    m->address = addr;
    m->next_poll_ms = now + APP_MOTOR_BOOT_MS;
}
/** @brief 判断无锁存故障且位置/状态反馈均在有效期内；不等同于已使能、已清零或已到位。 */
bool Motor_Healthy(const motor_t *m, uint32_t now)
{
    return !m->fault && !m->port->broken && m->position_valid && m->flags_valid &&
           now - m->position_ms < APP_MOTOR_STALE_MS && now - m->flags_ms < APP_MOTOR_STALE_MS;
}
/** @brief 任务侧置停止请求；Motor_Process 在发送缓冲可用时优先发停止，不在这里等待 ACK。 */
void Motor_RequestStop(motor_t *m)
{
    m->stop_requested = true;
}
/** @brief 满足通信恢复、反馈新鲜、无驱动器异常及无未完成停止时清除锁存故障。
 * 返回是否成功；不会重新建立原点，也不会自动恢复旧路径。 */
bool Motor_ClearFault(motor_t *m, uint32_t now)
{
    if (m->pending || m->stop_requested || m->stop_failed || m->port->broken || !m->flags_valid ||
        !m->position_valid || (m->flags & 0x0CU) || now - m->flags_ms >= APP_MOTOR_STALE_MS ||
        now - m->position_ms >= APP_MOTOR_STALE_MS)
        return false;
    m->fault = MOTOR_NO_FAULT;
    return true;
}
/** @brief 请求使能或失能；true 表示开始等待应答，最终以 ACK 和状态回读为准。
 * 忙、停止中或健康检查失败时返回 false，不在内部排队。 */
bool Motor_Enable(motor_t *m, bool en, uint32_t now)
{
    uint8_t b[16];
    if (m->fault || m->stop_requested || !Motor_Healthy(m, now))
        return false;
    if (!send(m, b, Emm_Enable(b, m->address, en), now))
        return false;
    m->pending_enable = en;
    return true;
}
/** @brief 请求当前位置清零；只允许非运动模式且反馈健康。原点先标无效，ACK 后等待新位置。
 * 仅限任务上下文；false 表示未下发。调用者须确认轴位于合适的静止参考位置。 */
bool Motor_Zero(motor_t *m, uint32_t now)
{
    uint8_t b[16];
    if (!Motor_Healthy(m, now) || m->stop_requested || m->mode == MOTOR_SPEED_MODE ||
        m->mode == MOTOR_POSITION_MODE)
        return false;
    m->origin_valid = false;
    return send(m, b, Emm_Zero(b, m->address), now);
}
/** @brief 集中检查运动前提：健康、原点有效、请求使能且驱动器反馈使能、无停止请求。 */
static bool can_move(motor_t *m, uint32_t now)
{
    return Motor_Healthy(m, now) && m->origin_valid && m->enable_requested && (m->flags & 1U) &&
           !m->stop_requested;
}
/** @brief 下发绝对位置命令；必须已使能、原点有效且反馈健康。pulses 为幅值，rpm 为转/分钟。
 * true 仅表示下发成功；到位由路径层结合新位置、状态与容差判定。 */
bool Motor_Move(motor_t *m, bool rev, uint16_t rpm, uint8_t acc, uint32_t pulses, uint32_t now)
{
    uint8_t b[16];
    if (!can_move(m, now) || rpm == 0)
        return false;
    if (!send(m, b, Emm_Position(b, m->address, rev, rpm, acc, pulses, true), now))
        return false;
    m->mode = MOTOR_POSITION_MODE;
    return true;
}
/** @brief 下发速度模式命令，方向和 RPM 已由上层决定；返回是否开始事务。
 * 此接口本身不执行物理行程约束，上层必须负责持续速度运动的边界与停止。 */
bool Motor_Velocity(motor_t *m, bool rev, uint16_t rpm, uint8_t acc, uint32_t now)
{
    uint8_t b[16];
    if (!can_move(m, now))
        return false;
    if (!send(m, b, Emm_Velocity(b, m->address, rev, rpm, acc), now))
        return false;
    m->mode = MOTOR_SPEED_MODE;
    return true;
}
/** @brief 只将匹配当前事务的应答写入反馈/状态；0xFD/0x9F 到位通知不能代替命令接受 ACK。 */
static void reply(motor_t *m, const emm_reply_t *r, uint32_t now)
{
    /* Only the response matching the active transaction updates feedback. */
    if (r->code == 0 && m->pending)
    {
        if (m->pending == EMM_STOP)
            m->stop_failed = true;
        m->rejected++;
        m->pending = 0;
        fault(m, MOTOR_REJECTED);
        return;
    }
    if (!m->pending || r->code != m->pending)
        return;
    if (r->code == EMM_MOVE && r->status == 0x9F)
        return; /* unsolicited arrival, not ACK */
    m->last_rx_ms = now;
    if (r->code == EMM_POSITION)
    {
        m->position = r->position;
        m->position_ms = now;
        m->position_valid = true;
    }
    else if (r->code == EMM_FLAGS)
    {
        m->flags = r->status;
        m->flags_ms = now;
        m->flags_valid = true;
        /* 状态 bit2/bit3 表示堵转及相关保护；bit0 使能、bit1 到位由上层检查。 */
        if (m->flags & 0x0CU)
            fault(m, MOTOR_STALL);
    }
    else if (r->code == EMM_SPEED)
        m->rpm = r->rpm;
    else if (r->status != EMM_ACK_OK)
    {
        m->rejected++;
        if (r->code == EMM_STOP)
            m->stop_failed = true;
        fault(m, MOTOR_REJECTED);
    }
    else
    {
        if (r->code == EMM_ENABLE)
        {
            m->enable_requested = m->pending_enable;
            m->mode = m->pending_enable ? MOTOR_HOLD : MOTOR_DISABLED;
        }
        if (r->code == EMM_ZERO)
        {
            m->origin_valid = true;
            m->position_valid = false;
        }
        if (r->code == EMM_STOP)
        {
            m->mode = MOTOR_HOLD;
            m->stop_failed = false;
        }
    }
    m->completed++;
    m->pending = 0;
    m->next_poll_ms = now + APP_MOTOR_POLL_MS;
}
/** @brief 处理有限数量接收字节、匹配应答、检查超时、恢复端口并优先推进停止。
 * 每轮 MotorTask 调用，now 为毫秒；不会自动重发不确定是否已执行的运动命令。 */
void Motor_Process(motor_t *m, uint32_t now)
{
    if (m->port->broken)
    {
        fault(m, MOTOR_COMM_FAULT);
        m->pending = 0;
        m->parser.used = 0;
        if (due(now, m->recovery_ms))
        {
            m->recovery_ms = now + 100U;
            (void)UART_BSP_Recover(m->port);
        }
        return;
    }
    if (m->parser.used && now - m->byte_ms > APP_MOTOR_REPLY_MS)
        m->parser.used = 0;
    uint8_t data[64];
    size_t n;
    /* Bounded RX budget so a noisy port cannot starve the other axes. */
    for (unsigned batch = 0; batch < 4 && (n = UART_BSP_Read(m->port, data, sizeof(data))) != 0;
         batch++)
    {
        m->byte_ms = now;
        for (size_t i = 0; i < n; i++)
        {
            emm_reply_t r;
            if (Emm_ParseByte(&m->parser, m->address, data[i], &r))
                reply(m, &r, now);
        }
    }
    /* 事务到期只能记录失败并恢复链路，不能假定电机未执行而再次发送运动。 */
    if (m->pending && now - m->pending_ms >= APP_MOTOR_REPLY_MS)
    {
        if (m->pending == EMM_STOP)
            m->stop_failed = true;
        m->timeouts++;
        m->pending = 0;
        m->parser.used = 0;
        fault(m, MOTOR_COMM_FAULT);
        /* Clear a stuck TX as well as stale bytes. Never retry uncertain motion. */
        m->port->broken = true;
        return;
    }
    if (m->enable_requested && m->position_valid && m->flags_valid &&
        (now - m->position_ms >= APP_MOTOR_STALE_MS || now - m->flags_ms >= APP_MOTOR_STALE_MS))
        fault(m, MOTOR_COMM_FAULT);
    /* 停止可取代等待应答事务，但先等待当前 DMA 发送完成，避免覆盖 TX 缓冲。 */
    if (m->stop_requested && !m->port->tx_busy)
    {
        uint8_t b[16];
        m->pending = 0;
        m->parser.used = 0;
        if (send(m, b, Emm_Stop(b, m->address), now))
            m->stop_requested = false;
        return;
    }
}
/* Queries run after motion dispatch, so polling never continually wins over commands. */
/** @brief 在业务命令分发之后轮询位置、状态、速度；忙时跳过，避免查询长期抢占运动命令。 */
void Motor_Poll(motor_t *m, uint32_t now)
{
    if (!m->pending && !m->stop_requested && due(now, m->next_poll_ms))
    {
        uint8_t b[16];
        const uint8_t codes[] = {EMM_POSITION, EMM_FLAGS, EMM_SPEED};
        if (send(m, b, Emm_Query(b, m->address, codes[m->poll_phase]), now))
            m->poll_phase = (m->poll_phase + 1U) % 3U;
    }
}
