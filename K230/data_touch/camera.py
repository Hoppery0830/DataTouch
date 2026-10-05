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

    def configure(self):
        self.sensor = Sensor(id=CAMERA_ID)
        self.sensor.reset()
        self.sensor.set_framesize(width=CAMERA_WIDTH, height=CAMERA_HEIGHT)
        self.sensor.set_pixformat(Sensor.RGB565)
        if self.sensor.width() != CAMERA_WIDTH or self.sensor.height() != CAMERA_HEIGHT:
            raise RuntimeError("unexpected camera resolution %dx%d" % (
                self.sensor.width(), self.sensor.height()))

    def start(self):
        if self.sensor is None:
            raise RuntimeError("camera is not configured")
        self.sensor.run()
        focus_result = self.sensor.focus_pos(FOCUS_POSITION)
        if focus_result is False:
            raise RuntimeError("unable to set focus position %d" % FOCUS_POSITION)
        self._started = True

    def initialize(self):
        """Preserve the accepted production configure -> run -> focus order."""
        self.configure()
        self.start()

    def snapshot(self):
        if not self._started:
            raise RuntimeError("camera is not started")
        return self.sensor.snapshot()

    def frame_to_gray(self, frame):
        """Apply the single frozen ROI/resize/Gray8 conversion path."""
        gray_roi = gray_256 = raw = None
        try:
            gray_roi = frame.to_grayscale(roi=CAMERA_ROI)
            gray_256 = gray_roi.scale(x_size=TARGET_SIZE, y_size=TARGET_SIZE)
            gray_roi = None
            raw = gray_256.to_numpy_ref()
            if int(raw.shape[0]) != TARGET_SIZE or int(raw.shape[1]) != TARGET_SIZE:
                raise RuntimeError("unexpected Gray8 shape %s" % (raw.shape,))
            return np.array(raw, dtype=np.float) / 255.0
        finally:
            gray_roi = None
            gray_256 = None
            raw = None

    def capture_gray(self):
        """Capture exactly one frame and return a copied 256x256 float Gray8 ROI."""
        if not self._started:
            raise RuntimeError("camera is not started")
        frame = None
        try:
            frame = self.snapshot()
            return self.frame_to_gray(frame)
        finally:
            frame = None

    def stop(self):
        if self._started and self.sensor is not None:
            self.sensor.stop()
            self._started = False
