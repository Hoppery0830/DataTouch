"""Debounced active-low KEY with distinct short- and long-press events."""

import time
from machine import FPIOA, Pin

from data_touch.config import (
    KEY_DEBOUNCE_MS,
    KEY_GPIO,
    KEY_LONG_PRESS_MS,
    KEY_PHYSICAL_PIN,
)


SHORT_PRESS = "SHORT"
LONG_PRESS = "LONG"


class KeyTrigger:
    def __init__(self, pin=None, clock=None):
        self._clock = clock if clock is not None else time
        if pin is None:
            fpioa = FPIOA()
            fpioa.set_function(KEY_PHYSICAL_PIN, getattr(FPIOA, "GPIO%d" % KEY_GPIO))
            pin = Pin(KEY_GPIO, Pin.IN, Pin.PULL_UP)
        self.pin = pin
        value = int(self.pin.value())
        self._stable = value
        self._candidate = value
        self._changed_ms = self._clock.ticks_ms()
        self._armed = value == 1
        self._pressed_ms = None
        self._long_reported = False

    def poll_event(self):
        value = int(self.pin.value())
        now = self._clock.ticks_ms()
        if value != self._candidate:
            self._candidate = value
            self._changed_ms = now
            return None
        if value != self._stable:
            if self._clock.ticks_diff(now, self._changed_ms) < KEY_DEBOUNCE_MS:
                return None
            self._stable = value
            if value == 0 and self._armed:
                self._armed = False
                self._pressed_ms = now
                self._long_reported = False
                return None
            if value == 1:
                event = None
                if self._pressed_ms is not None and not self._long_reported:
                    event = SHORT_PRESS
                self._pressed_ms = None
                self._long_reported = False
                self._armed = True
                return event
        if (self._stable == 0 and self._pressed_ms is not None
                and not self._long_reported
                and self._clock.ticks_diff(now, self._pressed_ms) >= KEY_LONG_PRESS_MS):
            self._long_reported = True
            return LONG_PRESS
        return None

    def poll(self):
        """Compatibility helper: True only for a completed short press."""
        return self.poll_event() == SHORT_PRESS
