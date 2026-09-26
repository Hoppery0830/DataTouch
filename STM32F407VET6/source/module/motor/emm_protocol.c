/**
 * @file emm_protocol.c
 * @brief Emm 固定 0x6B 模式的报文编码和流式解析；不依赖任务调度。
 * @see 本目录 README.md（职责、调用顺序与使用限制）。
 */
#include "emm_protocol.h"
#include <string.h>
/** @brief 将 16 位数按协议大端顺序写入缓冲，避免依赖 MCU 本机字节序。 */
static void be16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}
/** @brief 将 32 位数按协议大端顺序写入缓冲。 */
static void be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}
/** @brief 编码位置、转速或状态查询，返回帧长；不支持的功能返回 0，输出缓冲须至少 13B。 */
size_t Emm_Query(uint8_t *b, uint8_t a, uint8_t c)
{
    if (c != EMM_POSITION && c != EMM_SPEED && c != EMM_FLAGS)
        return 0;
    b[0] = a;
    b[1] = c;
    b[2] = EMM_TAIL;
    return 3;
}
/** @brief 编码使能/失能命令，返回 6B 帧长；输出缓冲须有效且至少 13B，不在此发送。 */
size_t Emm_Enable(uint8_t *b, uint8_t a, bool en)
{
    b[0] = a;
    b[1] = EMM_ENABLE;
    b[2] = 0xAB;
    b[3] = en;
    b[4] = 0;
    b[5] = EMM_TAIL;
    return 6;
}
/** @brief 编码立即停止命令，返回 5B 帧长；只生成报文，停止结果仍需应答确认。 */
size_t Emm_Stop(uint8_t *b, uint8_t a)
{
    b[0] = a;
    b[1] = EMM_STOP;
    b[2] = 0x98;
    b[3] = 0;
    b[4] = EMM_TAIL;
    return 5;
}
/** @brief 编码当前位置清零命令，返回 4B 帧长；这不是寻找机械原点。 */
size_t Emm_Zero(uint8_t *b, uint8_t a)
{
    b[0] = a;
    b[1] = EMM_ZERO;
    b[2] = 0x6D;
    b[3] = EMM_TAIL;
    return 4;
}
/** @brief 编码速度指令；rpm 为 0–5000 的整数转速，acc 为厂家参数，rev 为方向。
 * 返回 8B 帧长或 0（转速超范围）；多机同步标志固定关闭。 */
size_t Emm_Velocity(uint8_t *b, uint8_t a, bool rev, uint16_t rpm, uint8_t acc)
{
    if (rpm > 5000)
        return 0;
    b[0] = a;
    b[1] = EMM_VELOCITY;
    b[2] = rev;
    be16(b + 3, rpm);
    b[5] = acc;
    b[6] = 0;
    b[7] = EMM_TAIL;
    return 8;
}
/** @brief 编码位置指令；pulses 为脉冲幅值，rev 为符号方向，absolute 选择绝对/相对。
 * rpm 单位为转/分钟，返回 13B 帧长或 0；本项目上层运动采用绝对位置。 */
size_t Emm_Position(uint8_t *b, uint8_t a, bool rev, uint16_t rpm, uint8_t acc, uint32_t pulses,
                    bool absolute)
{
    if (rpm > 5000)
        return 0;
    b[0] = a;
    b[1] = EMM_MOVE;
    b[2] = rev;
    be16(b + 3, rpm);
    b[5] = acc;
    be32(b + 6, pulses);
    b[10] = absolute;
    b[11] = 0;
    b[12] = EMM_TAIL;
    return 13;
}
/** @brief 根据应答功能码确定固定帧长，未知功能返回 0。 */
static size_t length(uint8_t c)
{
    switch (c)
    {
    case EMM_POSITION:
        return 8;
    case EMM_SPEED:
        return 6;
    case 0x00:
    case EMM_FLAGS:
    case EMM_ENABLE:
    case EMM_MOVE:
    case EMM_VELOCITY:
    case EMM_STOP:
    case EMM_ZERO:
        return 4;
    default:
        return 0;
    }
}
/** @brief 移除一个候选字节并累计拒绝计数，以便重新寻找有效帧边界。 */
static void drop(emm_parser_t *p)
{
    memmove(p->data, p->data + 1, --p->used);
    p->rejected++;
}
/** @brief 逐字节喂入解析器；返回 true 时 r 得到一条完整应答，否则继续等待或重同步。
 * 检查地址、功能、长度和固定尾字节；失败滑动一字节，半帧超时由 motor 模块处理。 */
bool Emm_ParseByte(emm_parser_t *p, uint8_t addr, uint8_t byte, emm_reply_t *r)
{
    if (p->used == sizeof(p->data))
        drop(p);
    p->data[p->used++] = byte;
    while (p->used)
    {
        if (p->data[0] != addr)
        {
            drop(p);
            continue;
        }
        if (p->used < 2)
            return false;
        size_t n = length(p->data[1]);
        if (!n)
        {
            drop(p);
            continue;
        }
        /* Fixed 0x6B check mode; 0x00 error replies are dispatched to the active transaction. */
        if (p->used < n)
            return false;
        if (p->data[n - 1] != EMM_TAIL || ((n == 8 || n == 6) && p->data[2] > 1))
        {
            drop(p);
            continue;
        }
        memset(r, 0, sizeof(*r));
        r->code = p->data[1];
        r->status = p->data[2];
        if (n == 8)
        {
            uint32_t v = ((uint32_t)p->data[3] << 24) | ((uint32_t)p->data[4] << 16) |
                         ((uint32_t)p->data[5] << 8) | p->data[6];
            r->position = p->data[2] ? -(int64_t)v : (int64_t)v;
        }
        if (n == 6)
        {
            int32_t v = ((uint32_t)p->data[3] << 8) | p->data[4];
            r->rpm = p->data[2] ? -v : v;
        }
        p->used -= n;
        memmove(p->data, p->data + n, p->used);
        return true;
    }
    return false;
}
