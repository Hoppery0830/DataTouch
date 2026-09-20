import struct
import sys
import unittest
from pathlib import Path


DEPLOY_ROOT = Path(__file__).resolve().parents[1]
if str(DEPLOY_ROOT) not in sys.path:
    sys.path.insert(0, str(DEPLOY_ROOT))

from stm32_link import (  # noqa: E402
    LIGHT_ON,
    STM32Link,
    TYPE_ACK,
    TYPE_LIGHT_CONTROL,
    TYPE_U_CURVE,
    encode_frame,
    frame_checksum,
    pack_u_curve,
)


class FakeClock:
    def __init__(self):
        self.now = 0

    def ticks_ms(self):
        return self.now

    def ticks_diff(self, newer, older):
        return newer - older

    def sleep_ms(self, duration_ms):
        self.now += int(duration_ms)


class FakeUART:
    def __init__(self, on_write=None, initial_rx=b""):
        self.on_write = on_write
        self.rx = bytearray(initial_rx)
        self.writes = []

    def write(self, data):
        data = bytes(data)
        self.writes.append(data)
        if self.on_write is not None:
            reply = self.on_write(len(self.writes), data)
            if reply:
                self.rx.extend(reply)
        return len(data)

    def any(self):
        return len(self.rx)

    def read(self, size):
        data = bytes(self.rx[:size])
        del self.rx[:size]
        return data


class STM32ProtocolTests(unittest.TestCase):
    def test_light_frames_and_checksum(self):
        self.assertEqual(frame_checksum(TYPE_LIGHT_CONTROL, b"\x01"), 0x12)
        self.assertEqual(
            encode_frame(TYPE_LIGHT_CONTROL, b"\x01"),
            b"\xAA\x55\x10\x01\x01\x12",
        )
        self.assertEqual(
            encode_frame(TYPE_LIGHT_CONTROL, b"\x00"),
            b"\xAA\x55\x10\x01\x00\x11",
        )

    def test_u_curve_is_little_endian_float32(self):
        value = 0.1512816
        payload = pack_u_curve(value)
        self.assertEqual(payload, struct.pack("<f", value))
        self.assertEqual(len(payload), 4)
        self.assertAlmostEqual(struct.unpack("<f", payload)[0], value, places=7)
        frame = encode_frame(TYPE_U_CURVE, payload)
        self.assertEqual(frame[2], TYPE_U_CURVE)
        self.assertEqual(frame[3], 4)
        self.assertEqual(frame[-1], frame_checksum(TYPE_U_CURVE, payload))

    def test_receive_resynchronizes_after_noise_and_bad_checksum(self):
        bad = bytearray(encode_frame(TYPE_ACK, b"\x10\x00"))
        bad[-1] ^= 0x01
        good = encode_frame(TYPE_ACK, b"\x20\x00")
        uart = FakeUART(initial_rx=b"noise\xAA" + bytes(bad) + good)
        link = STM32Link(uart=uart, clock=FakeClock(), timeout_ms=20)
        self.assertEqual(link.receive_frame(), (TYPE_ACK, b"\x20\x00"))

    def test_receive_timeout_is_bounded(self):
        clock = FakeClock()
        link = STM32Link(uart=FakeUART(), clock=clock, timeout_ms=7)
        with self.assertRaises(TimeoutError):
            link.receive_frame()
        self.assertEqual(clock.now, 7)

    def test_impossible_length_does_not_block_resynchronization(self):
        poison = b"\xAA\x55\x10\xFFgarbage"
        good = encode_frame(TYPE_ACK, b"\x10\x00")
        link = STM32Link(
            uart=FakeUART(initial_rx=poison + good),
            clock=FakeClock(),
            timeout_ms=30,
        )
        self.assertEqual(link.receive_frame(), (TYPE_ACK, b"\x10\x00"))

    def test_send_with_ack_retries_then_succeeds(self):
        def respond(attempt, _frame):
            if attempt == 3:
                return encode_frame(TYPE_ACK, bytes((TYPE_LIGHT_CONTROL, 0)))
            return b""

        uart = FakeUART(on_write=respond)
        link = STM32Link(
            uart=uart,
            clock=FakeClock(),
            timeout_ms=4,
            max_attempts=3,
            retry_delay_ms=1,
        )
        self.assertTrue(link.light_on())
        self.assertEqual(len(uart.writes), 3)
        self.assertTrue(all(frame[4] == LIGHT_ON for frame in uart.writes))

    def test_send_with_ack_stops_after_three_timeouts(self):
        uart = FakeUART()
        link = STM32Link(
            uart=uart,
            clock=FakeClock(),
            timeout_ms=3,
            max_attempts=3,
            retry_delay_ms=1,
        )
        self.assertFalse(link.send_u_curve(0.25))
        self.assertEqual(len(uart.writes), 3)


if __name__ == "__main__":
    unittest.main()
