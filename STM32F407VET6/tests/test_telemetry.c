#include "touch_telemetry.h"
#include "touch_config.h"
#include "k230_link.h"
#include "remote_protocol.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static touch_features_t features;
static bool lamp;
static uint8_t lamp_status;
static uint8_t light(bool on)
{
    if (!lamp_status)
        lamp = on;
    return lamp_status;
}
static void feed(k230_link_t *s, uint8_t type, const uint8_t *p, unsigned n, uint32_t now)
{
    uint8_t sum = (uint8_t)(type + n), buf[14] = {0xaa, 0x55, type, (uint8_t)n};
    memcpy(buf + 4, p, n);
    for (unsigned i = 0; i < n; i++)
        sum += p[i];
    buf[4 + n] = sum;
    for (unsigned i = 0; i < n + 5; i++)
        K230_Feed(s, buf[i], now, light);
}
int main(void)
{
    touch_telemetry_t t = {0};
    touch_force_t f = {0};
    touch_adxl_t a = {0};
    f.values[0] = TOUCH_ZERO_FX_N + 3;
    f.values[1] = TOUCH_ZERO_FY_N + 4;
    f.values[2] = TOUCH_ZERO_FZ_N + 10;
    for (unsigned ms = 10; ms <= 2000; ms += 10)
    {
        f.timestamp_ms = ms;
        TouchFeatures_Force(&features, &f);
        a.rms_mg = 100;
        TouchFeatures_ADC(&features, &a, ms);
        TouchFeatures_Tick(&features, ms, &t);
    }
    assert(t.flags == 79);
    assert(t.seq == 20);
    assert(fabsf(t.v[6] - 5) < 0.001f);
    assert(fabsf(t.v[9] - 0.5f) < 0.001f && fabsf(t.v[15] - 0.5f) < 0.001f);
    assert(t.v[10] < 0.001f && t.v[11] < 0.001f && t.v[12] == 100 && t.v[13] == 100 &&
           t.v[19] == 0 && t.v[20] == 100);
    uint8_t wire[108], frame[133];
    TouchTelemetry_Encode(&t, wire);
    assert(wire[0] == 1 && RC_Read32(wire + 4) == 20);
    size_t n = RC_Build(frame, 0x21, wire, sizeof(wire));
    assert(n == 113);
    remote_parser_t parser = {0};
    for (unsigned i = 0; i < n; i++)
        assert(RC_Parse(&parser, frame[i], 2000) == (i == n - 1));
    assert(TouchFeatures_Tick(&features, 2100, &t));
    assert(!(t.flags & 15));
    a.clipped = true;
    TouchFeatures_ADC(&features, &a, 2200);
    TouchFeatures_Tick(&features, 2200, &t);
    assert((t.flags & 32) && !(t.flags & 2));
    memset(&features, 0, sizeof(features));
    for (unsigned ms = 10; ms <= 2000; ms += 10)
    {
        a.clipped = false;
        a.rms_mg = (uint32_t)(100 + 50 * sinf(6.2831853f * 5 * ms / 1000));
        TouchFeatures_ADC(&features, &a, ms);
        TouchFeatures_Tick(&features, ms, &t);
    }
    assert(t.flags & 64);
    assert(fabsf(t.v[19] - 5) < 0.1f);
    k230_link_t k = {0};
    uint8_t on = 1, off = 0;
    feed(&k, 0x10, &on, 1, 100);
    assert(lamp && k.pending && k.ack[5] == 0 && k.ack[6] == 0x92);
    feed(&k, 0x10, &on, 1, 200);
    assert(k.light_since == 100);
    K230_Tick(&k, 8100, light);
    assert(!lamp && !k.light);
    lamp_status = 2;
    feed(&k, 0x10, &on, 1, 8200);
    assert(!lamp && k.ack[5] == 2);
    lamp_status = 0;
    float u = 0.25f;
    uint8_t p[4];
    memcpy(p, &u, 4);
    feed(&k, 0x20, p, 4, 8300);
    assert(k.valid && k.u_curve == u && k.result_seq == 1 && k.ack[6] == 0xa2);
    feed(&k, 0x20, p, 4, 8400);
    assert(k.result_seq == 1);
    feed(&k, 0x20, p, 4, 11000);
    assert(k.result_seq == 2);
    u = NAN;
    memcpy(p, &u, 4);
    feed(&k, 0x20, p, 4, 11100);
    assert(k.result_seq == 2 && k.ack[5] == 1);
    K230_Feed(&k, 0xaa, 12000, light);
    feed(&k, 0x10, &off, 1, 12100);
    assert(k.ack[5] == 0);
    uint8_t done[9]={42,0,0,0,100,0,0,0,0};
    k.pending=false;
    feed(&k,0x31,done,9,12200);
    assert(k.capture_pending && !k.pending && !memcmp(k.capture_payload,done,9));
    k.capture_pending=false;
    feed(&k,0x31,done,8,12300);
    assert(!k.capture_pending);
    puts("PASS telemetry: force statistics, 10Hz, 2s windows, RMS envelope frequency, "
         "invalid/clipped, wire CRC; K230 ACK/retries/light timeout/NaN");
    return 0;
}
