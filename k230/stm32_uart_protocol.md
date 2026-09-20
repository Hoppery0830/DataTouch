# K230 - STM32 UART protocol

## Electrical connection

The production link is a dedicated 3.3 V TTL UART. It is separate from the
USB/COM14 REPL and upload connection.

| K230 | STM32 | Direction |
|---|---|---|
| GPIO11 / UART2 TX | UART RX | K230 to STM32 |
| GPIO12 / UART2 RX | UART TX | STM32 to K230 |
| GND | GND | Common reference |

Both devices must use 3.3 V logic. Do not connect either UART signal to 5 V.
The serial format is 115200 baud, 8 data bits, no parity, 1 stop bit (8N1).

## Frame format

All multibyte numeric payload fields use little-endian byte order.

```text
AA 55 | TYPE (1 byte) | LEN (1 byte) | PAYLOAD (LEN bytes) | CHECKSUM (1 byte)
```

```text
CHECKSUM = (TYPE + LEN + sum(PAYLOAD bytes)) & 0xFF
```

The receiver must scan for `AA 55`, reject a frame with a bad checksum, and
resume scanning for the next header. `LEN` is the payload length only.

## Commands sent by K230

### LIGHT_CONTROL (`TYPE = 0x10`)

The payload is one byte:

- `01`: turn the fill light ON.
- `00`: turn the fill light OFF.

Exact frames:

```text
LIGHT_ON  = AA 55 10 01 01 12
LIGHT_OFF = AA 55 10 01 00 11
```

### U_CURVE (`TYPE = 0x20`)

The payload is exactly four bytes: one IEEE-754 `float32` in little-endian
format. For example, the byte sequence can be decoded in C without alignment
assumptions as follows:

```c
uint32_t raw = ((uint32_t)p[0]) |
               ((uint32_t)p[1] << 8) |
               ((uint32_t)p[2] << 16) |
               ((uint32_t)p[3] << 24);
float u_curve;
memcpy(&u_curve, &raw, sizeof(u_curve));
```

## ACK sent by STM32

`TYPE = 0x80`, `LEN = 2`, payload `[acked_type, status]`.

- `acked_type`: the received command type (`0x10` or `0x20`).
- `status = 0x00`: command completed successfully.

Examples:

```text
ACK LIGHT_CONTROL OK = AA 55 80 02 10 00 92
ACK U_CURVE OK       = AA 55 80 02 20 00 A2
```

The K230 waits 500 ms for a matching OK ACK and sends a command at most three
times. Therefore, STM32 command handling must be idempotent: repeated ON leaves
the light on, repeated OFF leaves it off, and a repeated U_CURVE frame must be
ACKed without causing an unsafe side effect.

## Measurement transaction

```text
K230 KEY request
  -> LIGHT_ON
  <- ACK(0x10, OK)
  -> start the 5 s illumination window
  -> at t = 2 s, capture one CSI frame
  -> keep the light on until t = 5 s
  -> LIGHT_OFF
  <- ACK(0x10, OK)
  -> calculate frozen U_curve
  -> append timestamp,U_curve to the existing SD TXT
  -> U_CURVE(float32 little-endian)
  <- ACK(0x20, OK)
```

If LIGHT_ON is not acknowledged after three attempts, K230 attempts LIGHT_OFF,
cancels that measurement, and does not capture or append a result. If the
LIGHT_OFF ACK is missing after capture, K230 reports a warning but retains and
processes the already captured image.
