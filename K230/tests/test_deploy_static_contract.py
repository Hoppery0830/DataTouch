import sys
import unittest
from pathlib import Path


DEPLOY_ROOT = Path(__file__).resolve().parents[1]
if str(DEPLOY_ROOT) not in sys.path:
    sys.path.insert(0, str(DEPLOY_ROOT))

from data_touch import config  # noqa: E402


class DeployStaticContractTests(unittest.TestCase):
    def test_frozen_hardware_and_output_contract(self):
        self.assertEqual(config.CAMERA_ID, 1)
        self.assertEqual((config.CAMERA_WIDTH, config.CAMERA_HEIGHT), (1920, 1080))
        self.assertEqual(config.CAMERA_ROI, (240, 135, 1440, 810))
        self.assertEqual(config.TARGET_SIZE, 256)
        self.assertEqual(config.FOCUS_POSITION, 275)
        self.assertEqual((config.KEY_PHYSICAL_PIN, config.KEY_GPIO), (21, 21))
        self.assertEqual(config.KEY_LONG_PRESS_MS, 3000)
        self.assertEqual(config.LIGHT_CAPTURE_DELAY_MS, 2000)
        self.assertEqual(config.LIGHT_ON_DURATION_MS, 5000)
        self.assertEqual(config.RESULT_TXT, "/sdcard/data_touch_results/u_curve.txt")
        self.assertEqual(config.EXPECTED_BACKEND, "FUSED_SPAN_QUERY_NATIVE_C")
        self.assertEqual(config.DEBUG_JPEG_QUALITY, 100)
        self.assertEqual(config.PRODUCTION_PREVIEW_QUALITY, 100)
        self.assertEqual(config.PRODUCTION_PREVIEW_FPS, 30)
        self.assertGreaterEqual(config.DEBUG_CAMERA_WARMUP_MS, 1000)

    def test_measurement_call_order_is_frozen(self):
        source = (DEPLOY_ROOT / "data_touch" / "app.py").read_text(encoding="utf-8")
        measurement = source[source.index("def measure_once"):]
        calls = (
            "self.stm32.light_on()",
            "time.sleep_ms(LIGHT_CAPTURE_DELAY_MS)",
            "self.camera.capture_gray()",
            "remaining_light_ms = LIGHT_ON_DURATION_MS - elapsed_ms(light_started)",
            "time.sleep_ms(remaining_light_ms)",
            'self._try_light_off("after_light_window")',
            "evaluate_gray(gray)",
            "append_u_curve(value)",
            "self.stm32.send_u_curve(value)",
        )
        positions = [measurement.index(call) for call in calls]
        self.assertEqual(positions, sorted(positions))
        evaluator = source[source.index("def evaluate_gray"):source.index("class CameraApplication")]
        self.assertIn('if not result["finite"]', evaluator)

    def test_main_is_a_side_effect_free_mode_dispatcher(self):
        source = (DEPLOY_ROOT / "main.py").read_text(encoding="utf-8")
        self.assertIn("get_mode()", source)
        self.assertIn("K230_DEBUG_READY", source)
        self.assertNotIn("data_touch.camera", source)
        self.assertNotIn("stm32_link", source)
        self.assertNotIn("MediaManager", source)

    def test_production_has_explicit_takeover_and_safe_stop(self):
        source = (DEPLOY_ROOT / "data_touch" / "app.py").read_text(encoding="utf-8")
        self.assertIn('return "IDE interrupt" in str(error)', source)
        self.assertIn('IDE_EXPLICIT_TAKEOVER', source)
        self.assertIn('self.stop_requested = False', source)
        self.assertIn('self.measurement_busy = False', source)
        self.assertNotIn('source=IDE_ATTACH', source)

    def test_production_preview_is_single_owner_and_connection_gated(self):
        source = (DEPLOY_ROOT / "data_touch" / "app.py").read_text(encoding="utf-8")
        self.assertIn("machine.ide_connected()", source)
        self.assertIn("Display.VIRT", source)
        self.assertIn("to_ide=True", source)
        self.assertIn("self.camera.snapshot()", source)
        self.assertIn("Display.show_image(frame)", source)
        self.assertNotIn("_thread", source)
        self.assertEqual(source.count("self.camera = Camera()"), 1)

        machine_source = (DEPLOY_ROOT / "native" / "canmv_port" / "port" / "machine" / "modmachine.c").read_text(encoding="utf-8")
        self.assertIn("machine_ide_connected", machine_source)
        self.assertIn("ide_dbg_is_connected()", machine_source)

    def test_native_ide_attach_and_disconnect_are_observational(self):
        source = (DEPLOY_ROOT / "native" / "canmv_port" / "port" / "omv" / "ide_dbg.c").read_text(encoding="utf-8")
        attach = source[source.index("static void ide_dbg_route_protocol_start"):source.index("static void ide_dbg_route_bytes")]
        disconnect = source[source.index("static void ide_dbg_disconnect(void)"):source.index("void ide_dbg_interrupt")]
        self.assertNotIn("ide_dbg_request_soft_reset", attach)
        self.assertNotIn("ide_dbg_request_soft_reset", disconnect)
        self.assertIn("ide_dbg_request_debug_takeover", source)

    def test_debug_uses_shared_frozen_evaluator_and_max_quality(self):
        source = (DEPLOY_ROOT / "debug_main.py").read_text(encoding="utf-8")
        self.assertIn("from data_touch.app import evaluate_gray", source)
        self.assertNotIn("from multivariate_gunay_port import process", source)
        self.assertIn("Display.VIRT", source)
        self.assertIn("to_ide=True", source)
        self.assertIn("quality=DEBUG_JPEG_QUALITY", source)
        self.assertIn("def capture_once", source)
        self.assertIn("def evaluate_once", source)
        self.assertIn("def capture_evaluate_save", source)

    def test_mode_contract_and_software_reset(self):
        source = (DEPLOY_ROOT / "mode_config.py").read_text(encoding="utf-8")
        self.assertIn('MODE_PATH = "/sdcard/datatouch_mode.txt"', source)
        self.assertIn('MODE_RESET_FLAG_PATH = "/sdcard/.datatouch_mode_reset"', source)
        self.assertIn('marker = open(MODE_RESET_FLAG_PATH, "w")', source)
        self.assertNotIn("RTC_STATE_PATH", source)
        self.assertNotIn("persist_rtc", source)
        self.assertNotIn("restore_rtc", source)
        self.assertIn("return PRODUCTION", source)
        self.assertIn("machine.reset()", source)
        self.assertIn("blink_led(2, 300, 300)", source)
        self.assertIn("blink_led(4, 100, 100)", source)
        self.assertIn("blink_led(3, 100, 100)", source)

    def test_upload_manifest_contains_dual_mode_modules(self):
        source = (DEPLOY_ROOT / "upload_k230_final_deploy.ps1").read_text(encoding="utf-8")
        for name in (
            "main.py", "production_main.py", "debug_main.py", "mode_config.py",
            "set_debug_mode.py", "set_production_mode.py", "stm32_link.py",
        ):
            self.assertIn("'%s'" % name, source)


if __name__ == "__main__":
    unittest.main()
