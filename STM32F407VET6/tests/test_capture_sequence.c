#include "capture_sequence.h"
#include "touch_config.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static bool lamp, busy, fail_on, fail_off;
static unsigned requests, releases;
static uint8_t wire[14];
static uint8_t light(bool on)
{
    if ((on && fail_on) || (!on && fail_off))
        return 1;
    lamp = on;
    return 0;
}
static bool send(const uint8_t *p, size_t n)
{
    if (busy)
        return false;
    assert(n == p[3] + 5U);
    uint8_t checksum = 0;
    for (unsigned i = 2; i < n - 1; i++)
        checksum += p[i];
    assert(checksum == p[n - 1]);
    if (p[2] == CAPTURE_REQUEST)
    {
        requests++;
        assert(lamp);
    }
    else
    {
        releases++;
        if (p[12] == 0)
            assert(!lamp);
    }
    memcpy(wire, p, n);
    return true;
}
static void done(capture_sequence_t *s, uint8_t status, uint32_t now)
{
    uint8_t p[9];
    for (unsigned i = 0; i < 4; i++)
    {
        p[i] = s->seq >> (i * 8);
        p[i + 4] = s->epoch >> (i * 8);
    }
    p[8] = status;
    Capture_Done(s, p, 9, now, light);
}
int main(void)
{
    capture_sequence_t s;
    assert(Capture_Begin(&s, 42, 100, light) && lamp);
    Capture_Tick(&s, 2099, light, send);
    assert(!requests);
    busy = true;
    Capture_Tick(&s, 2100, light, send);
    assert(!requests && s.state == CAP_EXPOSURE);
    busy = false;
    Capture_Tick(&s, 2101, light, send);
    assert(requests == 1 && s.state == CAP_WAIT);
    Capture_Tick(&s, 2200, light, send);
    assert(requests == 1);
    uint8_t wrong[9] = {0};
    Capture_Done(&s, wrong, 9, 2201, light);
    assert(s.state == CAP_WAIT && lamp);
    done(&s, 0, 2202);
    assert(s.state == CAP_READY && !lamp);
    Capture_Tick(&s, 2203, light, send);
    assert(releases == 1 && wire[12] == 0);
    done(&s, 0, 2204);
    Capture_Tick(&s, 2205, light, send);
    assert(releases == 2 && requests == 1);
    assert(Capture_Begin(&s, 43, 3000, light));
    Capture_Tick(&s, 5000, light, send);
    done(&s, 1, 5001);
    assert(s.state == CAP_FAILED && s.error == CAP_CAMERA_ERROR && !lamp);
    assert(Capture_Begin(&s, 44, 6000, light));
    Capture_Tick(&s, 8000, light, send);
    done(&s, 0, 13000);
    assert(s.state == CAP_FAILED && s.error == CAP_TIMEOUT && !lamp);
    assert(Capture_Begin(&s, 45, 14000, light));
    Capture_Tick(&s, 16000, light, send);
    Capture_Cancel(&s, light);
    done(&s, 0, 16001);
    assert(s.state == CAP_FAILED && !lamp);
    fail_on = true;
    assert(!Capture_Begin(&s, 46, 17000, light));
    assert(s.error == CAP_LIGHT_ERROR && !lamp);
    fail_on = false;
    assert(Capture_Begin(&s, 47, 18000, light));
    Capture_Tick(&s, 20000, light, send);
    fail_off = true;
    done(&s, 0, 20001);
    assert(s.state == CAP_FAILED && s.light && lamp);
    fail_off = false;
    Capture_Tick(&s, 20002, light, send);
    assert(!lamp && s.state == CAP_FAILED && wire[12] == 1);
    assert(Capture_Begin(&s, 48, 21000, light));
    busy = true;
    Capture_Tick(&s, 29000, light, send);
    assert(s.error == CAP_TIMEOUT && !lamp);
    busy = false;
    assert(Capture_Begin(&s, 49, 0xfffffff0U, light));
    Capture_Tick(&s, 0x7c0U, light, send);
    assert(s.state == CAP_WAIT);
    done(&s, 0, 0x7c1U);
    assert(s.state == CAP_READY);
    puts("PASS capture: exposure/order, busy, wrong/duplicate token, failure, deadline, cancel, "
         "lamp error, tick wrap");
}
