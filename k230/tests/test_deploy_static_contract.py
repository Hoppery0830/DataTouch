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
        self.assertEqual((config.CAMERA_WIDTH, config.CAMERA_HEIGHT), (1280, 720))
        self.assertEqual(config.CAMERA_ROI, (160, 90, 960, 540))
        self.assertEqual(config.TARGET_SIZE, 256)
        self.assertEqual(config.FOCUS_POSITION, 275)
        self.assertEqual((config.KEY_PHYSICAL_PIN, config.KEY_GPIO), (21, 21))
        self.assertEqual(config.LIGHT_CAPTURE_DELAY_MS, 2000)
        self.assertEqual(config.LIGHT_ON_DURATION_MS, 5000)
        self.assertEqual(config.RESULT_TXT, "/sdcard/data_touch_results/u_curve.txt")
        self.assertEqual(config.EXPECTED_BACKEND, "FUSED_SPAN_QUERY_NATIVE_C")

    def test_measurement_call_order_is_frozen(self):
        source = (DEPLOY_ROOT / "data_touch" / "app.py").read_text(encoding="utf-8")
        calls = (
            "self.stm32.light_on()",
            "time.sleep_ms(LIGHT_CAPTURE_DELAY_MS)",
            "self.camera.capture_gray()",
            "remaining_light_ms = LIGHT_ON_DURATION_MS - elapsed_ms(light_started)",
            "time.sleep_ms(remaining_light_ms)",
            'self._try_light_off("after_light_window")',
            "process(gray)",
            "append_u_curve(value)",
            "self.stm32.send_u_curve(value)",
        )
        positions = [source.index(call) for call in calls]
        self.assertEqual(positions, sorted(positions))
        self.assertLess(source.index('if not result["finite"]'), source.index("append_u_curve(value)"))

    def test_upload_manifest_contains_uart_module(self):
        source = (DEPLOY_ROOT / "upload_k230_final_deploy.ps1").read_text(encoding="utf-8")
        self.assertIn("'stm32_link.py'", source)
        self.assertIn("'texture_structure_tensor_v1/structure_tensor_gunay.py'", source)
        self.assertNotIn(r"D:\K230\data_touch", source)


if __name__ == "__main__":
    unittest.main()
