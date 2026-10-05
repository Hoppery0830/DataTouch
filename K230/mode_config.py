"""Persistent dual-mode state, board LED signals and software reset."""

import time

from machine import FPIOA, Pin


PRODUCTION = "PRODUCTION"
DEBUG = "DEBUG"
PRODUCTION_RUNNING = "PRODUCTION_RUNNING"
DEBUG_IDLE = "DEBUG_IDLE"
DEBUG_RUNNING = "DEBUG_RUNNING"
MODE_PATH = "/sdcard/datatouch_mode.txt"
MODE_RESET_FLAG_PATH = "/sdcard/.datatouch_mode_reset"

LED_PHYSICAL_PIN = 52
LED_GPIO = 52


def get_mode():
    try:
        handle = open(MODE_PATH, "r")
        try:
            value = handle.read().strip().upper()
        finally:
            handle.close()
    except Exception:
        return PRODUCTION
    if value not in (PRODUCTION, DEBUG):
        return PRODUCTION
    return value


def set_mode(mode):
    value = str(mode).strip().upper()
    if value not in (PRODUCTION, DEBUG):
        raise ValueError("invalid DataTouch mode %s" % value)
    handle = open(MODE_PATH, "w")
    try:
        handle.write(value + "\n")
        handle.flush()
    finally:
        handle.close()

    # The native launcher consumes this one-shot marker after machine.reset().
    # It lets a deliberate mode switch run /sdcard/main.py once while an IDE is
    # attached, without colliding with later VS Code run-script soft resets.
    marker = open(MODE_RESET_FLAG_PATH, "w")
    try:
        marker.write(value + "\n")
        marker.flush()
    finally:
        marker.close()
    return value


def report_runtime_state(state, reason=None):
    if state not in (PRODUCTION_RUNNING, DEBUG_IDLE, DEBUG_RUNNING):
        raise ValueError("invalid DataTouch runtime state %s" % state)
    if reason is None:
        print("K230_RUNTIME_STATE %s" % state)
    else:
        print("K230_RUNTIME_STATE %s reason=%s" % (state, reason))


class BoardLED:
    def __init__(self):
        fpioa = FPIOA()
        fpioa.set_function(LED_PHYSICAL_PIN, getattr(FPIOA, "GPIO%d" % LED_GPIO))
        self.pin = Pin(LED_GPIO, Pin.OUT, pull=Pin.PULL_UP, drive=7)
        self.off()

    def on(self):
        self.pin.on()

    def off(self):
        self.pin.off()

    def blink(self, count, on_ms, off_ms):
        try:
            for _ in range(int(count)):
                self.on()
                time.sleep_ms(int(on_ms))
                self.off()
                time.sleep_ms(int(off_ms))
        finally:
            self.off()


def blink_led(count, on_ms, off_ms):
    led = BoardLED()
    led.blink(count, on_ms, off_ms)


def blink_production_start():
    blink_led(2, 300, 300)


def blink_debug_start():
    blink_led(4, 100, 100)


def blink_mode_confirmation():
    blink_led(3, 100, 100)


def software_reset():
    print("K230_SOFTWARE_RESET")
    time.sleep_ms(100)
    import machine

    machine.reset()
