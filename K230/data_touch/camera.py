"""One-shot CSI acquisition using the verified Real15 camera contract."""

from ulab import numpy as np
from media.sensor import Sensor

from data_touch.config import (
    CAMERA_HEIGHT,
    CAMERA_ID,
    CAMERA_ROI,
    CAMERA_WIDTH,
    FOCUS_POSITION,
    TARGET_SIZE,
)


class Camera:
    def __init__(self):
        self.sensor = None
        self._started = False

    def initialize(self):
        self.sensor = Sensor(id=CAMERA_ID)
        self.sensor.reset()
        self.sensor.set_framesize(width=CAMERA_WIDTH, height=CAMERA_HEIGHT)
        self.sensor.set_pixformat(Sensor.RGB565)
        if self.sensor.width() != CAMERA_WIDTH or self.sensor.height() != CAMERA_HEIGHT:
            raise RuntimeError("unexpected camera resolution %dx%d" % (
                self.sensor.width(), self.sensor.height()))
        self.sensor.run()
        focus_result = self.sensor.focus_pos(FOCUS_POSITION)
        if focus_result is False:
            raise RuntimeError("unable to set focus position %d" % FOCUS_POSITION)
        self._started = True

    def capture_gray(self):
        """Capture exactly one frame and return a copied 256x256 float Gray8 ROI."""
        if not self._started:
            raise RuntimeError("camera is not started")
        frame = gray_roi = gray_256 = raw = None
        try:
            frame = self.sensor.snapshot()
            gray_roi = frame.to_grayscale(roi=CAMERA_ROI)
            del frame
            frame = None
            gray_256 = gray_roi.scale(x_size=TARGET_SIZE, y_size=TARGET_SIZE)
            del gray_roi
            gray_roi = None
            raw = gray_256.to_numpy_ref()
            if int(raw.shape[0]) != TARGET_SIZE or int(raw.shape[1]) != TARGET_SIZE:
                raise RuntimeError("unexpected Gray8 shape %s" % (raw.shape,))
            gray = np.array(raw, dtype=np.float) / 255.0
            return gray
        finally:
            frame = None
            gray_roi = None
            gray_256 = None
            raw = None

    def stop(self):
        if self._started and self.sensor is not None:
            self.sensor.stop()
            self._started = False
