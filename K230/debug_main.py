"""Manual DEBUG entry point with preview, capture and frozen U_curve tools."""

import gc
import os
import time

from media.display import Display
from media.media import MediaManager

from data_touch.app import evaluate_gray
from data_touch.camera import Camera
from data_touch.config import (
    CAMERA_HEIGHT,
    CAMERA_WIDTH,
    DEBUG_CAMERA_WARMUP_MS,
    DEBUG_CAPTURE_DIRECTORY,
    DEBUG_JPEG_QUALITY,
    DEBUG_PREVIEW_FPS,
    LIGHT_CAPTURE_DELAY_MS,
    LIGHT_ON_DURATION_MS,
    RESULT_TXT,
)
from data_touch.result_log import (
    append_u_curve, ensure_directory, line_count, next_measurement_index, unused_path,
)
from stm32_link import STM32Link
from mode_config import DEBUG_IDLE, DEBUG_RUNNING, report_runtime_state

from st_utils import elapsed_ms, mem_free, now_ms


_capture_indices = {}


def _ensure_capture_directory():
    ensure_directory(DEBUG_CAPTURE_DIRECTORY)


def _capture_path(prefix="debug"):
    _ensure_capture_directory()
    index = _capture_indices.get(prefix, 1)
    while True:
        path = "%s/%s_%d.jpg" % (DEBUG_CAPTURE_DIRECTORY, prefix, index)
        try:
            os.stat(path)
        except OSError as error:
            if error.args and error.args[0] == 2:
                _capture_indices[prefix] = index + 1
                return path
            raise
        index += 1


def _measurement_path(index):
    _ensure_capture_directory()
    path = "%s/measure_%d.jpg" % (DEBUG_CAPTURE_DIRECTORY, index)
    # A power loss before logging may leave this uncommitted image. Preserve it
    # under an orphan name instead of overwriting it or blocking the next run.
    for candidate in (path, path + ".pending"):
        try:
            os.stat(candidate)
        except OSError as error:
            if error.args and error.args[0] == 2:
                continue
            raise
        archived = unused_path(candidate + ".orphan")
        os.rename(candidate, archived)
        print("K230_DEBUG_ORPHAN_IMAGE path=%s" % archived)
    return path


def _require_live_frame(frame):
    """Reject the known CSI failure mode instead of reporting a false PASS."""
    statistics = frame.get_statistics()
    if statistics.l_mean() == 0 and statistics.l_stdev() == 0:
        raise RuntimeError("CSI frame is all zero; reset board or check camera cable")


class DebugSession:
    def __init__(self, use_uart=False):
        self.camera = Camera()
        self.stm32 = None
        self._display_ready = False
        self._media_ready = False
        self._use_uart = bool(use_uart)

    def initialize(self):
        self.camera.configure()
        Display.init(
            Display.VIRT,
            width=CAMERA_WIDTH,
            height=CAMERA_HEIGHT,
            fps=DEBUG_PREVIEW_FPS,
            to_ide=True,
            quality=DEBUG_JPEG_QUALITY,
        )
        self._display_ready = True
        MediaManager.init()
        self._media_ready = True
        self.camera.start()
        # The first GC2093 frame after sensor.run() is a flat grey buffer.
        # Let streaming settle before any one-shot capture/evaluation.  This
        # changes neither the frozen CSI configuration nor the algorithm.
        time.sleep_ms(DEBUG_CAMERA_WARMUP_MS)
        # Drain initial buffers as well as waiting: the GC2093 can expose an
        # all-zero first buffer after sensor.run().
        warmup_frame = None
        for _ in range(8):
            warmup_frame = self.camera.snapshot()
            warmup_frame = None
            time.sleep_ms(20)
        if self._use_uart:
            self.stm32 = STM32Link()
        gc.collect()
        report_runtime_state(DEBUG_RUNNING, "DEBUG_SCRIPT")
        print("K230_DEBUG_SESSION_READY preview=VIRT quality=%d free_heap=%d" % (
            DEBUG_JPEG_QUALITY, mem_free()))

    def show(self, frame):
        Display.show_image(frame)

    def _try_light_off(self, context):
        try:
            acknowledged = self.stm32 is not None and self.stm32.light_off()
            if not acknowledged:
                print("K230_DEBUG_WARNING LIGHT_OFF_ACK_FAILED context=%s" % context)
            return acknowledged
        except Exception as error:
            print("K230_DEBUG_WARNING LIGHT_OFF_ERROR context=%s error=%s" % (context, error))
            return False

    def shutdown(self):
        try:
            if self.stm32 is not None:
                self._try_light_off("shutdown")
                self.stm32.close()
                self.stm32 = None
        finally:
            try:
                self.camera.stop()
            finally:
                if self._display_ready:
                    try:
                        Display.deinit()
                    finally:
                        self._display_ready = False
                if self._media_ready:
                    try:
                        MediaManager.deinit()
                    finally:
                        self._media_ready = False
                gc.collect()
                report_runtime_state(DEBUG_IDLE, "DEBUG_SCRIPT_RELEASED")
                print("K230_DEBUG_SESSION_RELEASED free_heap=%d" % mem_free())


def capture_once(save=True):
    """Capture one full-quality frame, publish it to VS Code and optionally save it."""
    session = DebugSession(use_uart=False)
    frame = None
    path = None
    try:
        session.initialize()
        frame = session.camera.snapshot()
        _require_live_frame(frame)
        if save:
            path = _capture_path("capture")
            frame.save(path, quality=DEBUG_JPEG_QUALITY)
        session.show(frame)
        time.sleep_ms(300)
        print("K230_DEBUG_CAPTURE PASS path=%s quality=%d" % (path, DEBUG_JPEG_QUALITY))
        return path
    finally:
        frame = None
        gc.collect()
        session.shutdown()


def evaluate_once():
    """Capture one ambient frame and evaluate the shared frozen U_curve core."""
    session = DebugSession(use_uart=False)
    frame = gray = result = None
    started = now_ms()
    try:
        session.initialize()
        frame = session.camera.snapshot()
        _require_live_frame(frame)
        gray = session.camera.frame_to_gray(frame)
        session.show(frame)
        frame = None
        gc.collect()
        result, value = evaluate_gray(gray)
        row = {
            "U_curve": value,
            "runtime_ms": elapsed_ms(started),
            "algorithm_ms": int(result["total_ms"]),
            "minimum_free_heap": int(result["min_free_bytes"]),
            "post_evaluate_heap": mem_free(),
        }
        print("K230_DEBUG_EVALUATE PASS U_curve=%.12g runtime_ms=%d algorithm_ms=%d min_heap=%d free_heap=%d" % (
            row["U_curve"], row["runtime_ms"], row["algorithm_ms"],
            row["minimum_free_heap"], row["post_evaluate_heap"]))
        return row
    finally:
        frame = None
        gray = None
        result = None
        gc.collect()
        session.shutdown()


def capture_evaluate_save():
    """Run the illuminated debug measurement with bounded image memory."""
    session = DebugSession(use_uart=True)
    frame = gray = result = None
    path = pending_path = None
    measurement_complete = False
    image_promoted = False
    light_off_attempted = False
    started = now_ms()
    try:
        session.initialize()
        index = next_measurement_index()
        path = _measurement_path(index)
        if not session.stm32.light_on():
            light_off_attempted = True
            session._try_light_off("light_on_ack_failed")
            raise RuntimeError("LIGHT_ON_ACK_FAILED")
        light_started = now_ms()
        time.sleep_ms(LIGHT_CAPTURE_DELAY_MS)
        frame = session.camera.snapshot()
        _require_live_frame(frame)
        # Persist the sensor frame while its backing buffer is still valid.
        # Image.compressed() may retain a reference to the live VB buffer on
        # this firmware, so keeping it across the 20 s native evaluation can
        # produce a valid JPEG container containing a flat grey frame.
        pending_path = path + ".pending"
        frame.save(pending_path, quality=DEBUG_JPEG_QUALITY)
        if os.stat(pending_path)[6] == 0:
            raise OSError("empty measurement image")
        image_handle = open(pending_path, "rb")
        try:
            image_handle.flush()
        finally:
            image_handle.close()
        gray = session.camera.frame_to_gray(frame)
        session.show(frame)
        frame = None
        gc.collect()

        remaining_light_ms = LIGHT_ON_DURATION_MS - elapsed_ms(light_started)
        if remaining_light_ms > 0:
            time.sleep_ms(remaining_light_ms)
        light_off_attempted = True
        if not session._try_light_off("after_light_window"):
            print("K230_DEBUG_WARNING continuing_with_captured_image")

        result, value = evaluate_gray(gray)
        gray = None
        gc.collect()

        os.rename(pending_path, path)
        image_promoted = True
        pending_path = None
        before_lines = line_count()
        line = append_u_curve(value, expected_index=index)
        measurement_complete = True
        after_lines = index
        row = {
            "index": index,
            "U_curve": value,
            "runtime_ms": elapsed_ms(started),
            "algorithm_ms": int(result["total_ms"]),
            "minimum_free_heap": int(result["min_free_bytes"]),
            "post_cleanup_heap": mem_free(),
            "image_path": path,
            "result_txt": RESULT_TXT,
            "txt_lines_before": before_lines,
            "txt_lines_after": after_lines,
            "line": line,
        }
        print("K230_DEBUG_MEASUREMENT PASS U_curve=%.12g runtime_ms=%d algorithm_ms=%d min_heap=%d image=%s lines=%d" % (
            row["U_curve"], row["runtime_ms"], row["algorithm_ms"],
            row["minimum_free_heap"], row["image_path"], row["txt_lines_after"]))
        return row
    finally:
        if not light_off_attempted:
            session._try_light_off("measurement_finally")
        frame = None
        gray = None
        result = None
        try:
            if not measurement_complete:
                cleanup_path = path if image_promoted else pending_path
                if cleanup_path is not None:
                    try:
                        os.remove(cleanup_path)
                    except OSError as error:
                        if not (error.args and error.args[0] == 2):
                            print("K230_DEBUG_WARNING IMAGE_CLEANUP_FAILED path=%s error=%s" % (cleanup_path, error))
        finally:
            gc.collect()
            session.shutdown()


def main(max_frames=None):
    """Publish live ambient-light preview until the user presses Ctrl-C."""
    session = DebugSession(use_uart=False)
    frame = None
    frame_count = 0
    try:
        session.initialize()
        print("K230_DEBUG_PREVIEW_RUNNING light=OFF quality=%d" % DEBUG_JPEG_QUALITY)
        while True:
            frame = session.camera.snapshot()
            if frame_count == 0:
                _require_live_frame(frame)
            session.show(frame)
            frame = None
            frame_count += 1
            if max_frames is not None and frame_count >= int(max_frames):
                print("K230_DEBUG_PREVIEW_STOPPED frames=%d" % frame_count)
                break
            try:
                os.exitpoint()
            except AttributeError:
                pass
            time.sleep_ms(10)
    except KeyboardInterrupt:
        print("K230_DEBUG_PREVIEW_STOPPED frames=%d" % frame_count)
    finally:
        frame = None
        gc.collect()
        session.shutdown()


if __name__ == "__main__":
    main()
