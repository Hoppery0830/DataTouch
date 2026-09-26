"""Bounded UART2 link between the K230 deployment and the STM32."""

import struct
import time


FRAME_HEADER = b"\xAA\x55"
TYPE_LIGHT_CONTROL = 0x10
TYPE_U_CURVE = 0x20
TYPE_ACK = 0x80

LIGHT_OFF = 0x00
LIGHT_ON = 0x01
STATUS_OK = 0x00

UART_TX_PIN = 11
UART_RX_PIN = 12
UART_BAUDRATE = 115200
ACK_TIMEOUT_MS = 500
MAX_ATTEMPTS = 3
RETRY_DELAY_MS = 20
RX_POLL_MS = 1
MAX_RX_PAYLOAD = 4


def frame_checksum(frame_type, payload):
    """Return the protocol checksum for TYPE, LEN and PAYLOAD."""
    payload = bytes(payload)
    if len(payload) > 255:
        raise ValueError("payload is too long")
    return (int(frame_type) + len(payload) + sum(payload)) & 0xFF


def encode_frame(frame_type, payload=b""):
    """Encode one AA55-framed protocol message."""
    frame_type = int(frame_type)
    if frame_type < 0 or frame_type > 255:
        raise ValueError("frame type must fit in one byte")
    payload = bytes(payload)
    checksum = frame_checksum(frame_type, payload)
    return FRAME_HEADER + bytes((frame_type, len(payload))) + payload + bytes((checksum,))


def pack_u_curve(value):
    """Pack U_curve as the fixed little-endian IEEE-754 float32 payload."""
    return struct.pack("<f", float(value))


class STM32Link:
    """UART2 transport with resynchronization, checksum checks and retries."""

    def __init__(self, uart=None, clock=None, timeout_ms=ACK_TIMEOUT_MS,
                 max_attempts=MAX_ATTEMPTS, retry_delay_ms=RETRY_DELAY_MS):
        if int(timeout_ms) <= 0:
            raise ValueError("timeout_ms must be positive")
        if int(max_attempts) <= 0:
            raise ValueError("max_attempts must be positive")

        self._clock = clock if clock is not None else time
        self.timeout_ms = int(timeout_ms)
        self.max_attempts = int(max_attempts)
        self.retry_delay_ms = int(retry_delay_ms)
        self._rx = bytearray()
        self._owns_uart = uart is None
        self._fpioa = None

        if uart is None:
            from machine import FPIOA, UART

            self._fpioa = FPIOA()
            self._fpioa.set_function(UART_TX_PIN, FPIOA.UART2_TXD)
            self._fpioa.set_function(UART_RX_PIN, FPIOA.UART2_RXD)
            uart = UART(
                UART.UART2,
                baudrate=UART_BAUDRATE,
                bits=UART.EIGHTBITS,
                parity=UART.PARITY_NONE,
                stop=UART.STOPBITS_ONE,
                timeout=0,
            )
        self.uart = uart

    def _ticks_ms(self):
        return self._clock.ticks_ms()

    def _ticks_diff(self, newer, older):
        ticks_diff = getattr(self._clock, "ticks_diff", None)
        if ticks_diff is None:
            return newer - older
        return ticks_diff(newer, older)

    def _sleep_ms(self, duration_ms):
        sleep_ms = getattr(self._clock, "sleep_ms", None)
        if sleep_ms is not None:
            sleep_ms(int(duration_ms))
        else:
            self._clock.sleep(float(duration_ms) / 1000.0)

    def send_frame(self, frame_type, payload=b""):
        """Write one complete frame or raise on a short UART write."""
        frame = encode_frame(frame_type, payload)
        written = self.uart.write(frame)
        if written != len(frame):
            raise OSError("short UART write %s/%d" % (written, len(frame)))
        return frame

    def _extract_frame(self):
        """Extract a valid frame, dropping noise or corrupt candidates."""
        while True:
            while self._rx and self._rx[0] != 0xAA:
                self._rx = self._rx[1:]
            if len(self._rx) < 2:
                return None
            if self._rx[1] != 0x55:
                self._rx = self._rx[1:]
                continue
            if len(self._rx) < 4:
                return None

            payload_length = self._rx[3]
            if payload_length > MAX_RX_PAYLOAD:
                self._rx = self._rx[1:]
                continue
            frame_length = 5 + payload_length
            if len(self._rx) < frame_length:
                return None

            frame_type = self._rx[2]
            payload = bytes(self._rx[4:4 + payload_length])
            checksum = self._rx[4 + payload_length]
            if checksum != frame_checksum(frame_type, payload):
                self._rx = self._rx[1:]
                continue

            self._rx = self._rx[frame_length:]
            return frame_type, payload

    def receive_frame(self, timeout_ms=None):
        """Receive one valid frame before a bounded timeout."""
        timeout_ms = self.timeout_ms if timeout_ms is None else int(timeout_ms)
        if timeout_ms <= 0:
            raise TimeoutError("UART receive timeout")
        started = self._ticks_ms()
        while self._ticks_diff(self._ticks_ms(), started) < timeout_ms:
            frame = self._extract_frame()
            if frame is not None:
                return frame

            available = int(self.uart.any())
            if available > 0:
                chunk = self.uart.read(available)
                if chunk:
                    self._rx.extend(chunk)
                    continue
            self._sleep_ms(RX_POLL_MS)
        raise TimeoutError("UART receive timeout")

    def wait_ack(self, acked_type, timeout_ms=None):
        """Wait for ACK[acked_type, status], ignoring unrelated valid frames."""
        timeout_ms = self.timeout_ms if timeout_ms is None else int(timeout_ms)
        started = self._ticks_ms()
        while True:
            remaining = timeout_ms - self._ticks_diff(self._ticks_ms(), started)
            if remaining <= 0:
                raise TimeoutError("ACK timeout for type 0x%02X" % int(acked_type))
            frame_type, payload = self.receive_frame(remaining)
            if frame_type != TYPE_ACK or len(payload) != 2:
                continue
            if payload[0] != int(acked_type):
                continue
            return payload[1] == STATUS_OK

    def send_with_ack(self, frame_type, payload=b""):
        """Send and await a matching OK ACK, with at most max_attempts sends."""
        self._rx = bytearray()
        for _ in range(256):
            available = int(self.uart.any())
            if available <= 0:
                break
            self.uart.read(available)
        for attempt in range(self.max_attempts):
            try:
                self.send_frame(frame_type, payload)
                if self.wait_ack(frame_type):
                    return True
            except Exception:
                pass
            if attempt + 1 < self.max_attempts:
                self._sleep_ms(self.retry_delay_ms)
        return False

    def light_on(self):
        return self.send_with_ack(TYPE_LIGHT_CONTROL, bytes((LIGHT_ON,)))

    def light_off(self):
        return self.send_with_ack(TYPE_LIGHT_CONTROL, bytes((LIGHT_OFF,)))

    def send_u_curve(self, value):
        return self.send_with_ack(TYPE_U_CURVE, pack_u_curve(value))

    def close(self):
        if self._owns_uart and self.uart is not None:
            self.uart.deinit()
