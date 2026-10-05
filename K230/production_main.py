"""Accepted production deployment plus a safe long-press mode switch."""

from mode_config import blink_production_start


def main(max_measurements=None):
    blink_production_start()
    from data_touch.app import main as production_application_main

    return production_application_main(max_measurements=max_measurements)


if __name__ == "__main__":
    main()
