"""Final one-shot KEY -> STM32 light -> CSI -> U_curve deployment."""

import gc
import machine
import sys
import time

from media.display import Display
from media.media import MediaManager

from data_touch.camera import Camera
from data_touch.config import (
    ALGORITHM_ROOT,
    EXPECTED_BACKEND,
    IDLE_POLL_MS,
    LIGHT_CAPTURE_DELAY_MS,
    LIGHT_ON_DURATION_MS,
    CAMERA_HEIGHT,
    CAMERA_WIDTH,
    PRODUCTION_PREVIEW_FPS,
    PRODUCTION_PREVIEW_QUALITY,
    RESULT_TXT,
)
from data_touch.key import KeyTrigger, LONG_PRESS, SHORT_PRESS
from data_touch.result_log import append_u_curve, initialize_log, line_count
from mode_config import (
    DEBUG,
    DEBUG_IDLE,
    PRODUCTION_RUNNING,
    blink_mode_confirmation,
    report_runtime_state,
    set_mode,
    software_reset,
)
from stm32_link import STM32Link

if ALGORITHM_ROOT not in sys.path:
    sys.path.insert(0, ALGORITHM_ROOT)

from multivariate_gunay_port import process
from st_utils import elapsed_ms, finite, mem_free, now_ms


def _is_ide_control_interrupt(error):
    return "IDE interrupt" in str(error)


def evaluate_gray(gray):
    """The only deployment wrapper around the frozen U_curve process()."""
    result = process(gray)
    backend = result["region_statistics_backend"]
    value = float(result["U_curve"])
    if backend != EXPECTED_BACKEND:
        raise RuntimeError("unexpected native backend %s" % backend)
    if not result["finite"] or not finite(value):
        raise RuntimeError("non-finite U_curve")
    return result, value


class CameraApplication:
    def __init__(self):
        self.camera = Camera()
        self.key = None
        self.stm32 = None
        self.measurement_busy = False
        self.measure_request = False
        self.mode_switch_requested = None
        self.stop_requested = False
        self.stop_reason = None
        self._shutdown_complete = False
        self._display_ready = False
        self._media_ready = False
        self._preview_connected = False

    def initialize(self):
        initialize_log()
        # Preserve the accepted configure -> run -> focus camera sequence while
        # adding a single VIRT display owned by Production.
        self.camera.configure()
        Display.init(
            Display.VIRT,
            width=CAMERA_WIDTH,
            height=CAMERA_HEIGHT,
            fps=PRODUCTION_PREVIEW_FPS,
            to_ide=True,
            quality=PRODUCTION_PREVIEW_QUALITY,
        )
        self._display_ready = True
        MediaManager.init()
        self._media_ready = True
        self.camera.start()
        self.stm32 = STM32Link()
        self.key = KeyTrigger()
        gc.collect()
        report_runtime_state(PRODUCTION_RUNNING, "BOOT")
        print("K230_DEPLOY_READY output=U_curve log=%s free_heap=%d" % (RESULT_TXT, mem_free()))

    def _preview_once(self):
        connected = bool(machine.ide_connected())
        if connected != self._preview_connected:
            self._preview_connected = connected
            print("K230_PRODUCTION_PREVIEW %s resolution=%dx%d quality=%d" % (
                "CONNECTED" if connected else "DISCONNECTED",
                CAMERA_WIDTH, CAMERA_HEIGHT, PRODUCTION_PREVIEW_QUALITY))
        if not connected or self.measurement_busy or self.stop_requested:
            return
        frame = None
        try:
            frame = self.camera.snapshot()
            Display.show_image(frame)
        except Exception as error:
            # Preview is observational: a transport/display failure must never
            # terminate Production or disable KEY measurements.
            print("K230_PRODUCTION_PREVIEW_WARNING error=%s" % error)
        finally:
            frame = None

    def request_stop(self, reason):
        if self.stop_requested:
            return
        self.stop_requested = True
        self.stop_reason = str(reason)
        self.measure_request = False
        print("K230_DEPLOY_STOP_REQUESTED reason=%s measurement_busy=%s" % (
            self.stop_reason, "true" if self.measurement_busy else "false"))

    def _try_light_off(self, context):
        try:
            acknowledged = self.stm32 is not None and self.stm32.light_off()
            if not acknowledged:
                print("K230_DEPLOY_WARNING LIGHT_OFF_ACK_FAILED context=%s" % context)
            return acknowledged
        except Exception as error:
            print("K230_DEPLOY_WARNING LIGHT_OFF_ERROR context=%s error=%s" % (context, error))
            return False

    def measure_once(self, index):
        if self.measurement_busy:
            return None
        self.measurement_busy = True
        gray = result = None
        light_off_attempted = False
        light_started = None
        started = None
        try:
            started = now_ms()
            if not self.stm32.light_on():
                light_off_attempted = True
                self._try_light_off("light_on_ack_failed")
                print("K230_DEPLOY_CANCELLED reason=LIGHT_ON_ACK_FAILED")
                return None

            light_started = now_ms()
            time.sleep_ms(LIGHT_CAPTURE_DELAY_MS)
            gray = self.camera.capture_gray()

            remaining_light_ms = LIGHT_ON_DURATION_MS - elapsed_ms(light_started)
            if remaining_light_ms > 0:
                time.sleep_ms(remaining_light_ms)

            light_off_attempted = True
            if not self._try_light_off("after_light_window"):
                print("K230_DEPLOY_WARNING continuing_with_captured_image")

            result, value = evaluate_gray(gray)
            before_lines = line_count()
            line = append_u_curve(value)
            after_lines = int(line.split(",", 1)[0])
            try:
                uart_ack = self.stm32.send_u_curve(value)
            except Exception as error:
                if _is_ide_control_interrupt(error):
                    raise
                uart_ack = False
                print("K230_DEPLOY_WARNING U_CURVE_SEND_ERROR saved_index=%d error=%s" % (after_lines, error))
            if not uart_ack:
                print("K230_DEPLOY_WARNING U_CURVE_ACK_FAILED U_curve=%.12g" % value)
            row = {
                "index": after_lines,
                "U_curve": value,
                "runtime_ms": elapsed_ms(started),
                "algorithm_ms": int(result["total_ms"]),
                "minimum_free_heap": int(result["min_free_bytes"]),
                "txt_lines_before": before_lines,
                "txt_lines_after": after_lines,
                "finite": True,
                "line": line,
                "uart_ack": uart_ack,
            }
            print("K230_DEPLOY_MEASUREMENT index=%d U_curve=%.12g runtime_ms=%d algorithm_ms=%d min_heap=%d lines=%d finite=PASS uart_ack=%s" % (
                row["index"], row["U_curve"], row["runtime_ms"], row["algorithm_ms"],
                row["minimum_free_heap"], after_lines, "PASS" if uart_ack else "WARNING"))
            return row
        except Exception as error:
            if _is_ide_control_interrupt(error):
                raise
            print("K230_DEPLOY_MEASUREMENT_ERROR index=%d error=%s" % (int(index), error))
            return None
        finally:
            if not light_off_attempted:
                self._try_light_off("measurement_finally")
            gray = None
            result = None
            self.measurement_busy = False
            gc.collect()
            print("K230_DEPLOY_READY output=U_curve log=%s free_heap=%d" % (RESULT_TXT, mem_free()))

    def run(self, max_measurements=None):
        completed = 0
        rows = []
        while (not self.stop_requested
               and (max_measurements is None or completed < max_measurements)):
            event = self.key.poll_event()
            if event == LONG_PRESS and not self.measurement_busy:
                set_mode(DEBUG)
                blink_mode_confirmation()
                self.mode_switch_requested = DEBUG
                self.request_stop("KEY_LONG_PRESS")
                print("K230_MODE_SWITCH target=DEBUG reset=PENDING")
                continue
            if event == SHORT_PRESS:
                if self.measurement_busy:
                    print("K230_DEPLOY_KEY_IGNORED reason=MEASUREMENT_BUSY")
                else:
                    self.measure_request = True
            if self.measure_request and not self.measurement_busy and not self.stop_requested:
                self.measure_request = False
                row = self.measure_once(completed + 1)
                if row is not None:
                    completed += 1
                    row["post_cleanup_heap"] = mem_free()
                    if max_measurements is not None:
                        rows.append(row)
                    print("K230_DEPLOY_IDLE completed=%d post_cleanup_heap=%d" % (
                        completed, row["post_cleanup_heap"]))
            self._preview_once()
            time.sleep_ms(IDLE_POLL_MS)
        print("K230_DEPLOY_ACCEPTANCE_DONE count=%d" % completed)
        return rows

    def shutdown(self):
        if self._shutdown_complete:
            return
        self._shutdown_complete = True
        self.stop_requested = True
        self.measure_request = False
        try:
            if self.stm32 is not None:
                self._try_light_off("shutdown")
        finally:
            try:
                if self.stm32 is not None:
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
                    self._preview_connected = False
                    self.key = None
                    self.measurement_busy = False
                    gc.collect()


def main(max_measurements=None):
    application = CameraApplication()
    rows = None
    debug_takeover = False
    try:
        application.initialize()
        rows = application.run(max_measurements=max_measurements)
    except KeyboardInterrupt:
        application.request_stop("KEYBOARD_INTERRUPT")
    except Exception as error:
        # The firmware emits this control exception only for an explicit
        # SCRIPT_STOP/Run takeover. Ordinary IDE attach/detach never reaches
        # this branch.
        if _is_ide_control_interrupt(error):
            debug_takeover = True
            application.request_stop("IDE_EXPLICIT_TAKEOVER")
        else:
            print("K230_DEPLOY_ERROR %s" % error)
            raise
    finally:
        application.shutdown()
    if debug_takeover:
        report_runtime_state(DEBUG_IDLE, "IDE_EXPLICIT_TAKEOVER")
        print("K230_DEPLOY_STOPPED reason=IDE_EXPLICIT_TAKEOVER resources=RELEASED")
        return None
    if application.mode_switch_requested == DEBUG:
        software_reset()
    return rows
