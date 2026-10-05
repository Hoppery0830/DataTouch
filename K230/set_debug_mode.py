"""Persist DEBUG mode and reboot without removing board power."""

from mode_config import DEBUG, blink_debug_start, set_mode, software_reset


set_mode(DEBUG)
blink_debug_start()
software_reset()
