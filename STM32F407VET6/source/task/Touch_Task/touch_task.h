/** @file touch_task.h
 * @brief 静态触觉采集任务；提供任务上下文快照，不控制电机、不创建采集会话。
 */
#pragma once
#include "touch_processing.h"
#include "touch_bsp.h"
typedef struct {
    touch_force_t force;
    touch_adxl_t adxl;
    touch_bsp_stats_t hardware;
    uint32_t adc_timestamp_ms, adc_blocks, crc_errors, invalid_values;
    bool force_online, adc_online;
} touch_status_t;
/* 便于 Ozone 直接观察；外部 C 代码使用 GetStatus 防止读到一半更新的数据。 */
extern volatile touch_status_t g_touch_status;
void TouchTask_Init(void);
void TouchTask_GetStatus(touch_status_t *out);
