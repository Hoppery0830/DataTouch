"""Append-only SD result log for the final U_curve output."""

import os
import time

from data_touch.config import RESULT_DIRECTORY, RESULT_TXT


def _ensure_directory():
    try:
        os.mkdir(RESULT_DIRECTORY)
    except OSError:
        pass


def timestamp():
    value = time.localtime()
    if value[0] < 2024:
        raise RuntimeError("RTC is not initialized")
    return "%04d-%02d-%02d %02d:%02d:%02d" % (
        value[0], value[1], value[2], value[3], value[4], value[5])


def append_u_curve(value):
    _ensure_directory()
    line = "%s,%.6f\n" % (timestamp(), float(value))
    handle = open(RESULT_TXT, "a")
    try:
        handle.write(line)
        try:
            handle.flush()
        except Exception:
            pass
    finally:
        handle.close()
    return line.rstrip()


def line_count():
    try:
        handle = open(RESULT_TXT, "r")
    except OSError:
        return 0
    try:
        return len(handle.read().splitlines())
    finally:
        handle.close()
