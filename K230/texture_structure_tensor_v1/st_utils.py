"""Minimal CanMV/ulab helpers for the frozen stream implementation."""
import gc
import math
import time

from ulab import numpy as np
import image

try:
    import ujson as json
except Exception:
    import json


def now_ms():
    return time.ticks_ms()


def elapsed_ms(start):
    return time.ticks_diff(time.ticks_ms(), start)


def collect():
    try:
        gc.collect()
    except Exception:
        pass


def mem_free():
    try:
        return int(gc.mem_free())
    except Exception:
        return -1


def reflect101(index, length):
    value = int(index)
    while value < 0 or value >= length:
        if value < 0:
            value = -value
        else:
            value = 2 * length - value - 2
    return value


def finite(value):
    value = float(value)
    return not (value != value or abs(value) == float("inf"))


def summary(array):
    # Diagnostics only, but keep them native too: scalar array indexing is
    # disproportionately expensive on this firmware.  Native std also avoids
    # cancellation in float32 sum-of-squares for low-variance Gaussian maps.
    return {"mean": float(np.mean(array)), "std": float(np.std(array))}


def load_frozen_gray(path, expected_height, expected_width):
    """Load only the PC-materialised uint8 gray ROI assets.

    A 2-D image avoids firmware RGB channel-order ambiguity.  If a firmware
    exposes it as RGB, a rounded BT.601 conversion reproduces cv2.cvtColor's
    uint8 output, but this fallback is diagnostic-only and recorded by caller.
    """
    # The frozen K4 file-validation bundle confirms the default constructor
    # decodes local SD images on this CanMV v1.8 firmware.  Supplying
    # copy_to_fb=True instead produced "image format not support" for the
    # same valid PNG payload; loading policy is an I/O compatibility detail,
    # not a change to the frozen Gray8 feature input.
    obj = image.Image(path)
    # The frozen Gray8 assets are carried as neutral RGB PNG solely because
    # this firmware's decoder does not expose RGB images through
    # ``to_numpy_ref``.  Its native grayscale conversion is exact for neutral
    # RGB (validated pixel-for-pixel against the host Gray8 container).
    raw = obj.to_grayscale().to_numpy_ref()
    if len(raw.shape) != 2:
        raise RuntimeError("expected 2-D grayscale decode, got %s" % (raw.shape,))
    gray = np.array(raw, dtype=np.float) / 255.0
    source = "NEUTRAL_RGB_TO_GRAY8_EXACT"
    if int(gray.shape[0]) != expected_height or int(gray.shape[1]) != expected_width:
        raise RuntimeError("expected %dx%d grayscale ROI, got %s" % (expected_width, expected_height, gray.shape))
    return gray, source


def read_manifest(path):
    handle = open(path, "r")
    lines = handle.read().splitlines()
    handle.close()
    if not lines:
        return []
    names = [name.lstrip("\ufeff") for name in lines[0].split(",")]
    rows = []
    for line in lines[1:]:
        values = line.split(",")
        rows.append(dict((name, values[index] if index < len(values) else "") for index, name in enumerate(names)))
    return rows


def write_json(path, payload):
    try:
        import os
        directory = path.rsplit("/", 1)[0]
        os.makedirs(directory)
    except Exception:
        pass
    handle = open(path, "w")
    json.dump(payload, handle)
    handle.close()
