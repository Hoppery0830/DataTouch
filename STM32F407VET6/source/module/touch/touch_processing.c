#include "touch_processing.h"
#include "touch_config.h"
#include <math.h>
#include <string.h>
#include <limits.h>

uint16_t Touch_CRC16(const uint8_t *data, size_t size)
{
    uint16_t crc = 0xffff;
    while (size--) {
        crc ^= *data++;
        for (unsigned i = 0; i < 8; i++)
            crc = (crc & 1) ? (uint16_t)((crc >> 1) ^ 0xa001) : (uint16_t)(crc >> 1);
    }
    return crc;
}
static void consume(touch_parser_t *p, unsigned n)
{
    p->used -= (uint8_t)n;
    memmove(p->bytes, p->bytes + n, p->used);
}
bool Touch_Parse(touch_parser_t *p, uint8_t byte, uint32_t now)
{
    if (!p) return false;
    if (now - p->last_byte_ms > 50U) p->used = 0;
    p->last_byte_ms = now;
    if (p->used == sizeof(p->bytes)) consume(p, 1);
    p->bytes[p->used++] = byte;
    while (p->used) {
        unsigned offset, size;
        touch_force_mode_t mode;
        if (p->bytes[0] == 0x53) {
            if (p->used < 2) return false;
            if (p->bytes[1] != 0x54) { consume(p, 1); continue; }
            offset = 2; size = 28; mode = TOUCH_FORCE_ACTIVE;
        } else if (p->bytes[0] == 1) {
            if (p->used < 3) return false;
            if (p->bytes[1] != 3 || p->bytes[2] != 24) { consume(p, 1); continue; }
            offset = 3; size = 29; mode = TOUCH_FORCE_MODBUS;
        } else { consume(p, 1); continue; }
        if (p->used < size) return false;
        uint16_t crc = (uint16_t)p->bytes[size - 2] | (uint16_t)p->bytes[size - 1] << 8;
        if (crc != Touch_CRC16(p->bytes, size - 2)) {
            p->crc_errors++; consume(p, 1); continue;
        }
        float v[6];
        bool finite = true;
        for (unsigned i = 0; i < 6; i++) {
            const uint8_t *b = p->bytes + offset + 4 * i;
            uint32_t bits = (uint32_t)b[0] | (uint32_t)b[1] << 8 |
                            (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
            memcpy(&v[i], &bits, sizeof(float));
            if (!isfinite(v[i])) finite = false;
        }
        consume(p, size);
        if (!finite) { p->invalid_values++; continue; }
        memcpy(p->latest.values, v, sizeof(v));
        p->latest.timestamp_ms = now;
        p->latest.count++;
        p->latest.mode = mode;
        p->latest.valid = true;
        return true;
    }
    return false;
}
int32_t Touch_Milli(float v)
{
    float m = v * 1000.0f;
    if (isnan(m)) return 0;
    if (m >= (float)INT32_MAX) return INT32_MAX;
    if (m <= (float)INT32_MIN) return INT32_MIN;
    return (int32_t)(m >= 0 ? m + 0.5f : m - 0.5f);
}
bool Touch_ProcessADC(const uint16_t *samples, size_t frames, touch_adxl_t *out)
{
    if (!samples || !out || !frames || frames > TOUCH_BLOCK_FRAMES) return false;
    const float zero[3] = {TOUCH_ZERO_X_RAW, TOUCH_ZERO_Y_RAW, TOUCH_ZERO_Z_RAW};
    const float sens[3] = {TOUCH_SENS_X_RAW_G, TOUCH_SENS_Y_RAW_G, TOUCH_SENS_Z_RAW_G};
    uint32_t sums[3] = {0};
    out->clipped = false;
    for (size_t i = 0; i < frames * 3; i++) {
        if (samples[i] > 4095) return false;
        sums[i % 3] += samples[i];
        if (samples[i] == 0 || samples[i] == 4095) out->clipped = true;
    }
    for (unsigned a = 0; a < 3; a++) {
        out->raw[a] = (uint16_t)(sums[a] / frames);
        out->mg[a] = Touch_Milli(((float)out->raw[a] - zero[a]) / sens[a]);
    }
    uint64_t sq = 0;
    for (size_t i = 0; i < frames * 3; i++) {
        int32_t d = (int32_t)samples[i] - out->raw[i % 3];
        sq += (uint32_t)(d * d);
    }
    /* 源工程使用 416.78 raw/g 及向下取整 RMS；不将重力/DC 分量计入振动。 */
    out->rms_mg = (uint32_t)sqrtf((float)(sq / frames) * (1000000.0f / (416.78f * 416.78f)));
    return true;
}
