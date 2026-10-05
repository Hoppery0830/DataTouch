"""Persist PRODUCTION mode and reboot without removing board power."""

from mode_config import PRODUCTION, blink_production_start, set_mode, software_reset


set_mode(PRODUCTION)
blink_production_start()
software_reset()
