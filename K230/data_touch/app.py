"""Final one-shot KEY -> STM32 light -> CSI -> U_curve deployment."""

import gc
import sys
import time

from data_touch.camera import Camera
from data_touch.config import (
    ALGORITHM_ROOT,
    EXPECTED_BACKEND,
    IDLE_POLL_MS,
    LIGHT_CAPTURE_DELAY_MS,
    LIGHT_ON_DURATION_MS,
    RESULT_TXT,
)
from data_touch.key import KeyTrigger
from data_touch.result_log import append_u_curve, line_count
from stm32_link import STM32Link

if ALGORITHM_ROOT not in sys.path:
    sys.path.insert(0, ALGORITHM_ROOT)

from multivariate_gunay_port import process
from st_utils import elapsed_ms, finite, mem_free, now_ms


class CameraApplication:
    def __init__(self):
        self.camera = Camera()
        self.key = None
        self.stm32 = None
        self.busy = False
        self.measure_request = False

    def initialize(self):
        self.camera.initialize()
        self.stm32 = STM32Link()
        self.key = KeyTrigger()
        gc.collect()
        print("K230_DEPLOY_READY output=U_curve log=%s free_heap=%d" % (RESULT_TXT, mem_free()))

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
        if self.busy:
            return None
        self.busy = True
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

            result = process(gray)
            backend = result["region_statistics_backend"]
            value = float(result["U_curve"])
            if backend != EXPECTED_BACKEND:
                raise RuntimeError("unexpected native backend %s" % backend)
            if not result["finite"] or not finite(value):
                raise RuntimeError("non-finite U_curve")
            before_lines = line_count()
            line = append_u_curve(value)
            after_lines = line_count()
            if after_lines != before_lines + 1:
                raise RuntimeError("SD append count mismatch %d -> %d" % (before_lines, after_lines))
            uart_ack = self.stm32.send_u_curve(value)
            if not uart_ack:
                print("K230_DEPLOY_WARNING U_CURVE_ACK_FAILED U_curve=%.12g" % value)
            row = {
                "index": int(index),
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
            print("K230_DEPLOY_MEASUREMENT_ERROR index=%d error=%s" % (int(index), error))
            return None
        finally:
            if not light_off_attempted:
                self._try_light_off("measurement_finally")
            gray = None
            result = None
            self.busy = False
            gc.collect()
            print("K230_DEPLOY_READY output=U_curve log=%s free_heap=%d" % (RESULT_TXT, mem_free()))

    def run(self, max_measurements=None):
        completed = 0
        rows = []
        while max_measurements is None or completed < max_measurements:
            if self.key.poll():
                self.measure_request = True
            if self.measure_request and not self.busy:
                self.measure_request = False
                row = self.measure_once(completed + 1)
                if row is not None:
                    completed += 1
                    row["post_cleanup_heap"] = mem_free()
                    if max_measurements is not None:
                        rows.append(row)
                    print("K230_DEPLOY_IDLE completed=%d post_cleanup_heap=%d" % (
                        completed, row["post_cleanup_heap"]))
            time.sleep_ms(IDLE_POLL_MS)
        print("K230_DEPLOY_ACCEPTANCE_DONE count=%d" % completed)
        return rows

    def shutdown(self):
        try:
            if self.stm32 is not None:
                self._try_light_off("shutdown")
        finally:
            try:
                if self.stm32 is not None:
                    self.stm32.close()
            finally:
                self.camera.stop()
                gc.collect()


def main(max_measurements=None):
    application = CameraApplication()
    try:
        application.initialize()
        return application.run(max_measurements=max_measurements)
    except KeyboardInterrupt:
        return None
    except Exception as error:
        print("K230_DEPLOY_ERROR %s" % error)
        raise
    finally:
        application.shutdown()
