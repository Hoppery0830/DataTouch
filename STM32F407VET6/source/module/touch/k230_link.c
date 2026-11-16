/**
 * @file k230_link.c
 * @brief 解析 K230 串口帧，维护采图通知、纹理结果和旧协议应答。
 */
#include "k230_link.h"
#include "capture_sequence.h"
#include <string.h>
#include <math.h>
void K230_Feed(k230_link_t *s, uint8_t b, uint32_t now, k230_light_fn light)
{
    if (s->used && now - s->last_byte > 50)
        s->used = 0;
    s->last_byte = now;
    if (s->used == sizeof(s->bytes)) {
        s->used = 0;
        s->bad_frames++;
    }
    s->bytes[s->used++] = b;
    while (s->used) {
        if (s->bytes[0] != 0xaa || (s->used > 1 && s->bytes[1] != 0x55))
            goto discard;
        if (s->used < 4)
            return;
        unsigned n = s->bytes[3] + 5;
        if (n > sizeof(s->bytes))
            goto discard;
        if (s->used < n)
            return;
        uint8_t sum = 0;
        for (unsigned i = 2; i < n - 1; i++)
            sum += s->bytes[i];
        if (sum != s->bytes[n - 1]) {
            s->bad_frames++;
            goto discard;
        }
        uint8_t type = s->bytes[2], len = s->bytes[3], status = 1;
        const uint8_t *p = s->bytes + 4;
        if (type == 0x33 && len == 12) {
            memcpy(s->visual_payload,p,12);s->visual_pending=true;s->used=0;return;
        }
        if (type == CAPTURE_DONE && len == 9) {
            memcpy(s->capture_payload, p, 9);
            s->capture_pending = true;
            s->used = 0;
            return;
        }
        if (type == 0x10 && len == 1 && p[0] <= 1) {
            status = light(p[0] != 0);
            if (!status) {
                if (p[0] && !s->light)
                    s->light_since = now;
                s->light = p[0] != 0;
            }
        } else if (type == 0x20 && len == 4) {
            uint32_t bits =
                (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
            float f;
            memcpy(&f, &bits, 4);
            if (isfinite(f) && fabsf(f) <= 1e6f) {
                // Protocol has no transaction ID. Suppress retry duplicates only within 2 seconds.
                if (!s->valid || f != s->u_curve || now - s->result_ms > 2000) {
                    s->u_curve = f;
                    s->result_ms = now;
                    s->result_seq++;
                }
                s->valid = true;
                status = 0;
            }
        }
        s->ack[0] = 0xaa;
        s->ack[1] = 0x55;
        s->ack[2] = 0x80;
        s->ack[3] = 2;
        s->ack[4] = type;
        s->ack[5] = status;
        s->ack[6] = (uint8_t)(0x82 + type + status);
        s->pending = true;
        s->used = 0;
        return;
discard:
        memmove(s->bytes, s->bytes + 1, --s->used);
    }
}
void K230_Tick(k230_link_t *s, uint32_t now, k230_light_fn light)
{
    // A lost LIGHT_OFF must not leave the lamp on indefinitely; duplicates do not extend it.
    if (s->light && now - s->light_since >= 8000 && light(false) == 0)
        s->light = false;
}
