"""Debounced active-low KEY with one-shot arming."""

import time
from machine import FPIOA, Pin

from data_touch.config import KEY_DEBOUNCE_MS, KEY_GPIO, KEY_PHYSICAL_PIN


class KeyTrigger:
    def __init__(self):
        fpioa = FPIOA()
        fpioa.set_function(KEY_PHYSICAL_PIN, getattr(FPIOA, "GPIO%d" % KEY_GPIO))
        self.pin = Pin(KEY_GPIO, Pin.IN, Pin.PULL_UP)
        value = int(self.pin.value())
        self._stable = value
        self._candidate = value
        self._changed_ms = time.ticks_ms()
        self._armed = value == 1

    def poll(self):
        value = int(self.pin.value())
        now = time.ticks_ms()
        if value != self._candidate:
            self._candidate = value
            self._changed_ms = now
            return False
        if value == self._stable:
            return False
        if time.ticks_diff(now, self._changed_ms) < KEY_DEBOUNCE_MS:
            return False
        self._stable = value
        if value == 1:
            self._armed = True
            return False
        if self._armed:
            self._armed = False
            return True
        return False
