/** @file tactile_result.h
 * @brief 按实验统计有效运动阶段的 100 ms 遥测块；结果等待 ESP32 持久化确认。
 */
#pragma once
#include "touch_telemetry.h"
#define TACTILE_RESULT_SIZE 88
typedef struct {
    bool active, pending;
    uint8_t mode, stage, quality, attempts, wire[TACTILE_RESULT_SIZE];
    uint32_t seq, epoch, start, phase_ms, last_seq, sent_ms, frames, fn, an, cn;
    double sum[6], normal, ft, mean, m2, contact_ft, contact_normal, rms_sq;
    float lo, hi, mu_peak, rms_peak;
} tactile_result_t;
/** @brief 开始一个实验，未确认的结果禁止覆盖。 */
bool Tactile_Begin(tactile_result_t *s, uint32_t seq, uint32_t epoch, uint8_t mode, uint32_t now);
/** @brief 更新实际运动阶段，阶段切换后跳过跨阶段的首个采样块。 */
void Tactile_Stage(tactile_result_t *s, uint8_t stage, uint32_t now);
/** @brief 纳入新鲜且完全位于目标阶段的遥测块，忽略重复样本。 */
void Tactile_Sample(tactile_result_t *s, const touch_telemetry_t *t, uint32_t now);
/** @brief 固化结果；outcome 为 0 完成、1 中断。无数据保留质量标志。 */
void Tactile_End(tactile_result_t *s, uint8_t outcome, uint32_t now);
/** @brief 最多十次、间隔一秒发送同一结果。 */
bool Tactile_Take(tactile_result_t *s, uint32_t now, uint8_t out[TACTILE_RESULT_SIZE]);
/** @brief 同令牌成功持久化确认才释放待发送结果。 */
void Tactile_Ack(tactile_result_t *s, const uint8_t ack[9]);
