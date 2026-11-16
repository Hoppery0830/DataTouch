/**
 * @file k230_link.h
 * @brief 解析 K230 串口帧，维护采图通知、纹理结果和旧协议应答。
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef struct
{
    uint8_t bytes[17], used, ack[7], capture_payload[9], visual_payload[12];
    bool visual_pending;
    bool capture_pending;
    bool pending, valid, light;
    uint32_t last_byte, result_ms, result_seq, bad_frames, light_since;
    float u_curve;
} k230_link_t;
/**
 * @brief 应用补光输出。
 * @param on true 开灯，false 关灯。
 * @return 输出操作成功返回 0。
 */
typedef uint8_t (*k230_light_fn)(bool on);
/**
 * @brief 解析一个 K230 串口字节，更新采图通知或纹理结果。
 * @param s K230 解析与状态缓存。
 * @param b 收到的字节。
 * @param now 当前 HAL 毫秒计数。
 * @param light 补光输出回调。
 */
void K230_Feed(k230_link_t *s, uint8_t b, uint32_t now, k230_light_fn light);
/**
 * @brief 维护 K230 链路的超时状态。
 * @param s K230 解析与状态缓存。
 * @param now 当前 HAL 毫秒计数。
 * @param light 补光输出回调。
 */
void K230_Tick(k230_link_t *s, uint32_t now, k230_light_fn light);
