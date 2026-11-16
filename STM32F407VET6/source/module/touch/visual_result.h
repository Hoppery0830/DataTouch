/**
 * @file visual_result.h
 * @brief 独立视觉结果的令牌校验、去重和有界转发；调用者负责并发保护。
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    uint8_t wire[12];
    bool valid, pending;
    uint8_t attempts;
    uint32_t sent_ms;
} visual_result_t;
/** @return 0 已接受或相同重发；1 令牌/数值不合法；2 转发槽忙。 */
uint8_t Visual_Accept(visual_result_t *s, const uint8_t wire[12],
                      uint32_t seq, uint32_t epoch, bool allowed);
/** @brief 生成下一次转发快照；最多十次，间隔 500ms。 */
bool Visual_Take(visual_result_t *s, uint32_t now, uint8_t out[12]);
/** @brief ESP32 持久化成功确认后释放转发槽，仍保留最近结果用于去重。 */
void Visual_Ack(visual_result_t *s, const uint8_t payload[9]);
/** @brief 指定实验的结果已经得到 ESP32 持久化确认。 */
bool Visual_Complete(const visual_result_t *s, uint32_t seq, uint32_t epoch);
