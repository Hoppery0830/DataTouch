#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef struct
{
    uint8_t bytes[9], used, ack[7];
    bool pending, valid, light;
    uint32_t last_byte, result_ms, result_seq, bad_frames, light_since;
    float u_curve;
} k230_link_t;
/* Return 0 only after the light output was actually applied. */
typedef uint8_t (*k230_light_fn)(bool on);
void K230_Feed(k230_link_t *s, uint8_t b, uint32_t now, k230_light_fn light);
void K230_Tick(k230_link_t *s, uint32_t now, k230_light_fn light);
