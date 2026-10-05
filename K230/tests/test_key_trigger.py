import importlib
import sys
import types
import unittest
from pathlib import Path


DEPLOY_ROOT = Path(__file__).resolve().parents[1]
if str(DEPLOY_ROOT) not in sys.path:
    sys.path.insert(0, str(DEPLOY_ROOT))

fake_machine = types.ModuleType("machine")
fake_machine.FPIOA = object
fake_machine.Pin = object
sys.modules.setdefault("machine", fake_machine)

key_module = importlib.import_module("data_touch.key")


class FakeClock:
    def __init__(self):
        self.now = 0

    def ticks_ms(self):
        return self.now

    def ticks_diff(self, newer, older):
        return newer - older

    def advance(self, milliseconds):
        self.now += int(milliseconds)


class FakePin:
    def __init__(self, value=1):
        self.current = int(value)

    def value(self):
        return self.current


class KeyTriggerTests(unittest.TestCase):
    def settle(self, trigger, clock):
        trigger.poll_event()
        clock.advance(key_module.KEY_DEBOUNCE_MS)
        return trigger.poll_event()

    def test_short_press_is_reported_on_release(self):
        clock = FakeClock()
        pin = FakePin(1)
        trigger = key_module.KeyTrigger(pin=pin, clock=clock)
        pin.current = 0
        self.assertIsNone(self.settle(trigger, clock))
        clock.advance(100)
        pin.current = 1
        self.assertEqual(self.settle(trigger, clock), key_module.SHORT_PRESS)

    def test_long_press_fires_once_while_held(self):
        clock = FakeClock()
        pin = FakePin(1)
        trigger = key_module.KeyTrigger(pin=pin, clock=clock)
        pin.current = 0
        self.assertIsNone(self.settle(trigger, clock))
        clock.advance(key_module.KEY_LONG_PRESS_MS)
        self.assertEqual(trigger.poll_event(), key_module.LONG_PRESS)
        clock.advance(1000)
        self.assertIsNone(trigger.poll_event())
        pin.current = 1
        self.assertIsNone(self.settle(trigger, clock))


if __name__ == "__main__":
    unittest.main()
