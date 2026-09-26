/**
 * @file emm_protocol.h
 * @brief Emm 固定 0x6B 模式的报文编码和流式解析；不依赖任务调度。
 * @see 本目录 README.md（职责、调用顺序与使用限制）。
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#define EMM_TAIL 0x6BU
#define EMM_ACK_OK 0x02U
#define EMM_POSITION 0x36U
#define EMM_SPEED 0x35U
#define EMM_FLAGS 0x3AU
#define EMM_ENABLE 0xF3U
#define EMM_MOVE 0xFDU
#define EMM_VELOCITY 0xF6U
#define EMM_STOP 0xFEU
#define EMM_ZERO 0x0AU
/* Functions write at most 13 bytes; caller supplies a buffer >=13 bytes. */
/** @brief 编码位置、转速或状态查询，返回帧长；不支持的功能返回 0，输出缓冲须至少 13B。 */
size_t Emm_Query(uint8_t *out, uint8_t addr, uint8_t code);
/** @brief 编码使能/失能命令，返回 6B 帧长；输出缓冲须有效且至少 13B，不在此发送。 */
size_t Emm_Enable(uint8_t *out, uint8_t addr, bool enable);
/** @brief 编码立即停止命令，返回 5B 帧长；只生成报文，停止结果仍需应答确认。 */
size_t Emm_Stop(uint8_t *out, uint8_t addr);
/** @brief 编码当前位置清零命令，返回 4B 帧长；这不是寻找机械原点。 */
size_t Emm_Zero(uint8_t *out, uint8_t addr);
/** @brief 编码速度指令；rpm 为 0–5000 的整数转速，acc 为厂家参数，rev 为方向。
 * 返回 8B 帧长或 0（转速超范围）；多机同步标志固定关闭。 */
size_t Emm_Velocity(uint8_t *out, uint8_t addr, bool reverse, uint16_t rpm, uint8_t acc);
/** @brief 编码位置指令；pulses 为脉冲幅值，rev 为符号方向，absolute 选择绝对/相对。
 * rpm 单位为转/分钟，返回 13B 帧长或 0；本项目上层运动采用绝对位置。 */
size_t Emm_Position(uint8_t *out, uint8_t addr, bool reverse, uint16_t rpm, uint8_t acc,
                    uint32_t pulses, bool absolute);
typedef struct
{
    uint8_t code, status;
    int64_t position;
    int32_t rpm;
} emm_reply_t;
typedef struct
{
    uint8_t data[16];
    size_t used;
    uint32_t rejected;
} emm_parser_t;
/** @brief 逐字节喂入解析器；返回 true 时 r 得到一条完整应答，否则继续等待或重同步。
 * 检查地址、功能、长度和固定尾字节；失败滑动一字节，半帧超时由 motor 模块处理。 */
bool Emm_ParseByte(emm_parser_t *p, uint8_t addr, uint8_t byte, emm_reply_t *reply);
