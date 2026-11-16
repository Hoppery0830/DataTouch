/**
 * @file capture_sequence.c
 * @brief 先采图后动作的补光、曝光等待、采图确认与取消状态机。
 */
#include "capture_sequence.h"
#include "touch_config.h"
#include <string.h>
static uint32_t read32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static size_t frame(const capture_sequence_t *s, uint8_t type, uint8_t out[14])
{
    out[0] = 0xaa;
    out[1] = 0x55;
    out[2] = type;
    out[3] = type == CAPTURE_REQUEST ? 8 : 9;
    for (unsigned i = 0; i < 4; i++) {
        out[4 + i] = (uint8_t)(s->seq >> (8 * i));
        out[8 + i] = (uint8_t)(s->epoch >> (8 * i));
    }
    if (out[3] == 9)
        out[12] = s->state == CAP_READY ? 0 : 1;
    uint8_t sum = 0;
    for (unsigned i = 2; i < 4U + out[3]; i++)
        sum += out[i];
    out[4 + out[3]] = sum;
    return out[3] + 5U;
}
static void finish(capture_sequence_t *s, uint8_t error, capture_light_fn light)
{
    /* READY is published only after the OFF output operation succeeds. */
    if (light(false) != 0) {
        error = CAP_LIGHT_ERROR;
        s->light = true;
    } else
        s->light = false;
    s->error = error;
    s->state = error == CAP_OK ? CAP_READY : CAP_FAILED;
    s->reply_pending = true;
}
bool Capture_Begin(capture_sequence_t *s, uint32_t seq, uint32_t now, capture_light_fn light)
{
    memset(s, 0, sizeof(*s));
    s->seq = seq;
    s->epoch = s->since = now;
    if (light(true) != 0) {
        finish(s, CAP_LIGHT_ERROR, light);
        return false;
    }
    s->light = true;
    s->state = CAP_EXPOSURE;
    return true;
}
void Capture_Tick(capture_sequence_t *s, uint32_t now, capture_light_fn light, capture_send_fn send)
{
    uint8_t wire[14];
    if ((s->state == CAP_EXPOSURE || s->state == CAP_WAIT) &&
        now - s->epoch >= TOUCH_CAPTURE_TOTAL_MS)
        finish(s, CAP_TIMEOUT, light);
    if (s->state == CAP_EXPOSURE && now - s->since >= TOUCH_EXPOSURE_MS) {
        size_t n = frame(s, CAPTURE_REQUEST, wire);
        /* Retry only HAL-busy; after the first accepted send never request a second photo. */
        if (send(wire, n)) {
            s->state = CAP_WAIT;
            s->since = now;
        }
    }
    if (s->state == CAP_WAIT && now - s->since >= TOUCH_CAPTURE_TIMEOUT_MS)
        finish(s, CAP_TIMEOUT, light);
    if (s->state == CAP_FAILED && s->light && light(false) == 0)
        s->light = false;
    if (s->reply_pending) {
        size_t n = frame(s, CAPTURE_RELEASE, wire);
        if (send(wire, n))
            s->reply_pending = false;
    }
}
void Capture_Done(capture_sequence_t *s, const uint8_t *p, size_t n, uint32_t now,
                  capture_light_fn light)
{
    if (n != 9 || read32(p) != s->seq || read32(p + 4) != s->epoch)
        return;
    if (s->state == CAP_WAIT) {
        if (now - s->since >= TOUCH_CAPTURE_TIMEOUT_MS || now - s->epoch >= TOUCH_CAPTURE_TOTAL_MS)
            finish(s, CAP_TIMEOUT, light);
        else
            finish(s, p[8] == 0 ? CAP_OK : CAP_CAMERA_ERROR, light);
    } else if (s->state == CAP_READY || s->state == CAP_FAILED) {
        /* A retransmitted notification only replays the terminal response. */
        s->reply_pending = true;
    }
}
void Capture_Cancel(capture_sequence_t *s, capture_light_fn light)
{
    finish(s, CAP_CANCELLED, light);
}
