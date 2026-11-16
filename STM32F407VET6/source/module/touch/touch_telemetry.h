/**
 * @file touch_telemetry.h
 * @brief 聚合六轴力与振动数据，生成定长触觉遥测负载。
 */
#pragma once
#include "touch_processing.h"
#define TOUCH_TELEMETRY_TYPE 0x21
#define TOUCH_TELEMETRY_SIZE 108
#define TOUCH_TELEMETRY_VALUES 22
/* Flags: force, ADC, force-window, contact, K230-result, clipped, ADC-window. */
typedef struct
{
    uint32_t seq, ms, k_seq, k_ms;
    uint8_t flags, light;
    float v[22];
} touch_telemetry_t;
typedef struct
{
    uint32_t output_ms, window_ms, seq, fn, an, wn, wa, last_force, last_adc;
    float sums[6], ft_sum, normal_sum, mu_sum, rms_sq;
    float window_ft[1200], window_normal_sum, window_ft_sum, window_mu_max;
    float envelope[220], features[13];
    bool clipped, window_bad_force, window_bad_adc, force_features, adc_features;
} touch_features_t;
/**
 * @brief 向遥测窗口累计一个六轴力样本。
 * @param s 特征累计状态。
 * @param f 本次力与力矩样本。
 */
void TouchFeatures_Force(touch_features_t *s, const touch_force_t *f);
/**
 * @brief 向遥测窗口累计一个 ADXL335 数据块。
 * @param s 特征累计状态。
 * @param a 本次 ADC 块的振动统计。
 * @param ms 数据块的毫秒时间戳。
 */
void TouchFeatures_ADC(touch_features_t *s, const touch_adxl_t *a, uint32_t ms);
/**
 * @brief 窗口到期时生成一帧触觉遥测。
 * @param s 特征累计状态。
 * @param now 当前 HAL 毫秒计数。
 * @param out 接收遥测数据的缓冲区。
 * @return 有新遥测输出时返回 true。
 */
bool TouchFeatures_Tick(touch_features_t *s, uint32_t now, touch_telemetry_t *out);
/**
 * @brief 以小端字节序编码 V1 触觉遥测负载。
 * @param s 要编码的遥测数据。
 * @param out 容量至少为 TOUCH_TELEMETRY_SIZE 字节的目标缓冲区。
 */
void TouchTelemetry_Encode(const touch_telemetry_t *s, uint8_t out[TOUCH_TELEMETRY_SIZE]);
