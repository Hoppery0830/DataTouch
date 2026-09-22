/** @file remote_protocol.h
 * @brief 三端控制协议 V1：AA55/type/len/payload/CRC8；多字节数小端。见 README.md。
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#define RC_REQUEST 0x10
#define RC_REPLY 0x11
#define RC_STATUS 0x12
#define RC_HEARTBEAT 0x13
#define RC_MAX_PAYLOAD 24
/* 请求 payload: seq:u32, op:u8, mode:u8；回复: seq:u32,result:u8。
 * 状态: state,mode,stage,online_mask,fault_mask (5 bytes)。 */
enum
{
    RC_SELECT = 0,
    RC_ARM,
    RC_START,
    RC_STOP,
    RC_RESET
};
enum
{
    RC_ACCEPTED = 0,
    RC_COMPLETE,
    RC_BUSY,
    RC_INVALID,
    RC_NOT_READY,
    RC_FAULT,
    RC_CANCELLED
};
enum
{
    RC_IDLE = 0,
    RC_ARMED,
    RC_RUNNING,
    RC_DONE,
    RC_ERROR,
    RC_PREPARING,
    RC_STOPPING
};
typedef struct
{
    uint32_t seq;
    uint8_t op, mode;
} remote_request_t;
typedef struct
{
    uint32_t seq;
    uint8_t result;
} remote_reply_t;
typedef struct
{
    uint8_t state, mode, stage, online, fault;
} remote_status_t;
typedef struct
{
    uint8_t bytes[RC_MAX_PAYLOAD + 5];
    uint8_t used;
    uint32_t last_ms;
} remote_parser_t;
static inline uint32_t RC_Read32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static inline void RC_Write32(uint8_t *p, uint32_t v)
{
    for (unsigned i = 0; i < 4; i++)
        p[i] = (uint8_t)(v >> (8 * i));
}
static inline uint8_t RC_Crc(const uint8_t *p, size_t n)
{
    uint8_t c = 255;
    while (n--)
    {
        c ^= *p++;
        for (unsigned i = 0; i < 8; i++)
            c = (c & 128) ? (uint8_t)((c << 1) ^ 0x31) : (uint8_t)(c << 1);
    }
    return c;
}
size_t RC_Build(uint8_t *out, uint8_t type, const uint8_t *payload, uint8_t len);
/** 返回完整有效帧，数据位于 parser.bytes；坏帧/半帧超时自动恢复。 */
bool RC_Parse(remote_parser_t *p, uint8_t b, uint32_t now);
