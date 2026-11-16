/** @file visual_result.c
 * @brief K230 结果去重与 ESP32 转发，不作为运动启动条件。
 */
#include "visual_result.h"
#include <math.h>
#include <string.h>
static uint32_t read32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
uint8_t Visual_Accept(visual_result_t *s, const uint8_t wire[12],
                      uint32_t seq, uint32_t epoch, bool allowed)
{
    uint32_t bits=read32(wire+8);
    float value;
    memcpy(&value,&bits,4);
    if (!isfinite(value) || value<0 || value>1e6f) return 1;
    if (s->valid && !memcmp(wire,s->wire,8))
        return memcmp(wire,s->wire,12) ? 1 : 0;
    if (!allowed || read32(wire)!=seq || read32(wire+4)!=epoch) return 1;
    if (s->pending) return 2;
    memcpy(s->wire,wire,12);
    s->valid=s->pending=true;
    s->attempts=0;
    return 0;
}
bool Visual_Take(visual_result_t *s, uint32_t now, uint8_t out[12])
{
    if (!s->pending || s->attempts>=10 || (s->attempts && now-s->sent_ms<500)) return false;
    memcpy(out,s->wire,12);
    s->attempts++;
    s->sent_ms=now;
    return true;
}
void Visual_Ack(visual_result_t *s, const uint8_t p[9])
{
    if (s->valid && !p[8] && !memcmp(s->wire,p,8)) s->pending=false;
}
bool Visual_Complete(const visual_result_t *s, uint32_t seq, uint32_t epoch)
{
    return s->valid && !s->pending && read32(s->wire)==seq && read32(s->wire+4)==epoch;
}
