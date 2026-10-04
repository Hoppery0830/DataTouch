/** @file touch_processing.h
 * @brief 无 HAL/RTOS 依赖的六维力解帧和 ADXL335 块统计，所有对象由调用任务独占。
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef enum { TOUCH_FORCE_UNKNOWN, TOUCH_FORCE_ACTIVE, TOUCH_FORCE_MODBUS } touch_force_mode_t;
typedef struct {
    float values[6]; /* Fx,Fy,Fz:N; Mx,My,Mz:N*m，未扣零偏 */
    uint32_t timestamp_ms, count;
    touch_force_mode_t mode;
    bool valid;
} touch_force_t;
typedef struct {
    uint8_t bytes[29], used;
    uint32_t last_byte_ms, crc_errors, invalid_values;
    touch_force_t latest;
} touch_parser_t;
typedef struct {
    int32_t mg[3];
    uint32_t rms_mg;
    uint16_t raw[3];
    bool clipped;
} touch_adxl_t;
uint16_t Touch_CRC16(const uint8_t *data, size_t size);
/* 半帧超时 50ms 自动丢弃；CRC 错误逐字节重同步；拒绝 NaN/Inf。 */
bool Touch_Parse(touch_parser_t *p, uint8_t byte, uint32_t now_ms);
/* 输入交错 X/Y/Z 的 12bit ADC；保留原示例整数均值、平均灵敏度 RMS 算法。 */
bool Touch_ProcessADC(const uint16_t *samples, size_t frames, touch_adxl_t *out);
/* 饱和取整，避免异常传感器值转换整型产生未定义行为。 */
int32_t Touch_Milli(float value);
