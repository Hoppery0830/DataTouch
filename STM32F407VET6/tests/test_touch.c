#include "touch_processing.h"
#include "touch_config.h"
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static size_t frame(uint8_t *b, bool modbus, const float v[6])
{
    size_t off = modbus ? 3 : 2, size = off + 26;
    b[0] = modbus ? 1 : 0x53; b[1] = modbus ? 3 : 0x54;
    if (modbus) b[2] = 24;
    for (unsigned i = 0; i < 6; i++) {
        uint32_t bits;
        memcpy(&bits, v + i, sizeof(bits));
        for (unsigned j = 0; j < 4; j++) b[off + i * 4 + j] = (uint8_t)(bits >> (j * 8));
    }
    uint16_t crc = Touch_CRC16(b, size - 2);
    b[size - 2] = (uint8_t)crc; b[size - 1] = (uint8_t)(crc >> 8);
    return size;
}
static unsigned feed(touch_parser_t *p, const uint8_t *b, size_t n, uint32_t now)
{
    unsigned count = 0;
    for (size_t i = 0; i < n; i++) count += Touch_Parse(p, b[i], now);
    return count;
}
static void protocol(void)
{
    assert(Touch_CRC16((const uint8_t *)"123456789", 9) == 0x4b37);
    uint8_t b[29];
    float v[6] = {1, -2, 3.25f, 0.125f, -0.5f, 6};
    touch_parser_t p = {0};
    size_t n = frame(b, false, v);
    assert(feed(&p, b, 7, 10) == 0);
    assert(feed(&p, b + 7, n - 7, 12) == 1);
    assert(p.latest.valid && p.latest.timestamp_ms == 12 && p.latest.count == 1);
    assert(p.latest.mode == TOUCH_FORCE_ACTIVE);
    for (unsigned i = 0; i < 6; i++) assert(p.latest.values[i] == v[i]);
    b[n - 1] ^= 0x40;
    assert(feed(&p, b, n, 20) == 0 && p.crc_errors == 1);
    n = frame(b, true, v);
    assert(feed(&p, b, n, 30) == 1 && p.latest.mode == TOUCH_FORCE_MODBUS);
    assert(feed(&p, b, n, 30) == 1 && p.latest.count == 3);
    v[2] = NAN; n = frame(b, false, v);
    assert(feed(&p, b, n, 40) == 0 && p.invalid_values == 1 && p.latest.count == 3);
    v[2] = INFINITY; n = frame(b, true, v);
    assert(feed(&p, b, n, 40) == 0 && p.invalid_values == 2);
    v[2] = 4; n = frame(b, false, v);
    feed(&p, b, 5, 50);
    assert(feed(&p, b, n, 150) == 1); /* 丢失半帧超时后恢复 */
    memset(&p, 0, sizeof(p));
    assert(feed(&p, b, 10, UINT32_MAX - 2) == 0);
    assert(feed(&p, b + 10, n - 10, 2) == 1); /* HAL 毫秒计数回绕 */
    for (unsigned i = 0; i < 2000; i++) Touch_Parse(&p, 0xff, 3);
    assert(feed(&p, b, n, 4) == 1 && p.used == 0);
}
static void adc(void)
{
    uint16_t b[TOUCH_BLOCK_FRAMES * 3];
    touch_adxl_t out;
    for (unsigned i = 0; i < TOUCH_BLOCK_FRAMES; i++) {
        b[3 * i] = 1973; b[3 * i + 1] = 1980; b[3 * i + 2] = 2386;
    }
    assert(Touch_ProcessADC(b, TOUCH_BLOCK_FRAMES, &out));
    assert(out.mg[0] == 0 && out.mg[1] == -1 && out.mg[2] == 999);
    assert(out.rms_mg == 0 && !out.clipped);
    for (unsigned i = 0; i < TOUCH_BLOCK_FRAMES; i++) b[3 * i] = (i & 1) ? 2100 : 1900;
    assert(Touch_ProcessADC(b, TOUCH_BLOCK_FRAMES, &out));
    assert(out.raw[0] == 2000 && out.rms_mg == 239); /* sqrt(mean(+-100)^2)/416.78 */
    b[0] = 4095;
    assert(Touch_ProcessADC(b, TOUCH_BLOCK_FRAMES, &out) && out.clipped);
    b[0] = 4096;
    assert(!Touch_ProcessADC(b, TOUCH_BLOCK_FRAMES, &out));
    assert(!Touch_ProcessADC(b, 0, &out));
    assert(!Touch_ProcessADC(b, TOUCH_BLOCK_FRAMES + 1, &out));
    assert(!Touch_ProcessADC(NULL, 1, &out));
    assert(Touch_Milli(1.5f) == 1500 && Touch_Milli(-1.5f) == -1500);
    assert(Touch_Milli(INFINITY) == INT32_MAX && Touch_Milli(-INFINITY) == INT32_MIN);
    assert(Touch_Milli(NAN) == 0);
}
int main(void)
{
    protocol(); adc();
    puts("PASS: touch CRC, active/Modbus fragmentation, recovery, NaN/Inf, tick wrap, ADC calibration/RMS/clipping");
    return 0;
}
