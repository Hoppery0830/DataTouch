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
void TouchFeatures_Force(touch_features_t *s, const touch_force_t *f);
void TouchFeatures_ADC(touch_features_t *s, const touch_adxl_t *a, uint32_t ms);
bool TouchFeatures_Tick(touch_features_t *s, uint32_t now, touch_telemetry_t *out);
void TouchTelemetry_Encode(const touch_telemetry_t *s, uint8_t out[TOUCH_TELEMETRY_SIZE]);
