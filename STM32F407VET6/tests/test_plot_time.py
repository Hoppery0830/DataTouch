"""Plot time regression tests, without opening serial ports or importing the GUI."""
import ast
from pathlib import Path
import unittest

script = Path(__file__).resolve().parents[1] / 'tools/SixAxis_ADXL335_realtime_plot.py'
tree = ast.parse(script.read_text(encoding='utf-8-sig'))
function = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == 'elapsed_seconds')
namespace = {}
exec(compile(ast.Module(body=[function], type_ignores=[]), str(script), 'exec'), namespace)
elapsed_seconds = namespace['elapsed_seconds']


class PlotTimeTests(unittest.TestCase):
    def test_adc_precedes_first_force_frame(self):
        # Actual captured stream: F timestamp=20, followed by A timestamp=11.
        self.assertAlmostEqual(elapsed_seconds(11, 20), -0.009)
        self.assertAlmostEqual(elapsed_seconds(31, 20), 0.011)

    def test_tick_wrap(self):
        self.assertAlmostEqual(elapsed_seconds(3, 0xFFFFFFFE), 0.005)
        self.assertAlmostEqual(elapsed_seconds(0xFFFFFFFE, 3), -0.005)

    def test_ordinary_session(self):
        self.assertEqual(elapsed_seconds(20, 20), 0)
        self.assertAlmostEqual(elapsed_seconds(30421, 20), 30.401)


if __name__ == '__main__':
    unittest.main()
