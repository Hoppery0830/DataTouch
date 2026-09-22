#include "remote_protocol.h"
#include <string.h>
size_t RC_Build(uint8_t *out, uint8_t type, const uint8_t *payload, uint8_t len)
{
    if (len > RC_MAX_PAYLOAD)
        return 0;
    out[0] = 0xAA;
    out[1] = 0x55;
    out[2] = type;
    out[3] = len;
    if (len)
        memcpy(out + 4, payload, len);
    out[4 + len] = RC_Crc(out, 4 + len);
    return len + 5;
}
bool RC_Parse(remote_parser_t *p, uint8_t b, uint32_t now)
{
    if (p->used && now - p->last_ms > 50)
        p->used = 0;
    p->last_ms = now;
    if (!p->used)
    {
        if (b == 0xAA)
            p->bytes[p->used++] = b;
        return false;
    }
    if (p->used == 1 && b != 0x55)
    {
        p->used = b == 0xAA ? 1 : 0;
        return false;
    }
    p->bytes[p->used++] = b;
    if (p->used == 4 && b > RC_MAX_PAYLOAD)
    {
        p->used = 0;
        return false;
    }
    if (p->used >= 5 && p->used == p->bytes[3] + 5)
    {
        bool ok = RC_Crc(p->bytes, p->used - 1) == b;
        p->used = 0;
        return ok;
    }
    return false;
}
