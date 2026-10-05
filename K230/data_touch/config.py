"""Frozen final K230 deployment configuration."""

CAMERA_ID = 1
CAMERA_WIDTH = 1920
CAMERA_HEIGHT = 1080
# Preserve the accepted ROI field of view after the 1280x720 -> 1920x1080
# sensor upgrade by scaling every coordinate and extent by 1.5.
CAMERA_ROI = (240, 135, 1440, 810)
TARGET_SIZE = 256
FOCUS_POSITION = 275

KEY_PHYSICAL_PIN = 21
KEY_GPIO = 21
KEY_DEBOUNCE_MS = 30
KEY_LONG_PRESS_MS = 3000
IDLE_POLL_MS = 20

LIGHT_CAPTURE_DELAY_MS = 2000
LIGHT_ON_DURATION_MS = 5000

ALGORITHM_ROOT = "/sdcard/texture_structure_tensor_v1"
RESULT_DIRECTORY = "/sdcard/data_touch_results"
RESULT_TXT = RESULT_DIRECTORY + "/u_curve.txt"

EXPECTED_BACKEND = "FUSED_SPAN_QUERY_NATIVE_C"

DEBUG_CAPTURE_DIRECTORY = "/sdcard/debug_capture"
DEBUG_JPEG_QUALITY = 100
DEBUG_PREVIEW_FPS = 30
DEBUG_CAMERA_WARMUP_MS = 1000

# Production publishes the same 1080P CSI stream to the IDE while attached.
# This does not change the sensor parameters or the frozen measurement path.
PRODUCTION_PREVIEW_FPS = 30
PRODUCTION_PREVIEW_QUALITY = 100
