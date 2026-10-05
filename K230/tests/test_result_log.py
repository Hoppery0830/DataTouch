import ast
import builtins
import contextlib
import io
import os
import re
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch


DEPLOY_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(DEPLOY_ROOT))
from data_touch import result_log


def source_objects(file, names, namespace):
    tree = ast.parse((DEPLOY_ROOT / file).read_text(encoding="utf-8"))
    nodes = [node for node in tree.body if getattr(node, "name", None) in names]
    exec(compile(ast.Module(body=nodes, type_ignores=[]), file, "exec"), namespace)
    return namespace


class FaultFile:
    def __init__(self, handle, fault):
        self.handle, self.fault = handle, fault

    def __getattr__(self, name):
        return getattr(self.handle, name)

    def write(self, data):
        if self.fault == "short":
            return self.handle.write(data[:3])
        return self.handle.write(data)

    def flush(self):
        if self.fault == "flush":
            raise OSError("injected flush failure")
        return self.handle.flush()

    def close(self):
        self.handle.close()
        if self.fault == "close":
            raise OSError("injected close failure")

    def read(self, length=-1):
        data = self.handle.read(length)
        return b"x" * len(data) if self.fault == "readback" else data


class ResultLogTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="datatouch_log_")
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name) / "results"
        self.path = self.directory / "u_curve.txt"
        self.stack = contextlib.ExitStack()
        self.addCleanup(self.stack.close)
        self.stack.enter_context(patch.object(result_log, "RESULT_DIRECTORY", str(self.directory)))
        self.stack.enter_context(patch.object(result_log, "RESULT_TXT", str(self.path)))
        self.stack.enter_context(patch.object(result_log, "_state", None))
        self.stack.enter_context(contextlib.redirect_stdout(io.StringIO()))

    def seed(self, text):
        self.directory.mkdir(exist_ok=True)
        self.path.write_text(text, encoding="ascii")

    def test_numbering_continues_after_restart(self):
        self.assertEqual(result_log.append_u_curve(0.581634569168), "1,0.581635")
        self.assertEqual(result_log.append_u_curve(0.25), "2,0.250000")
        result_log._state = None
        self.assertEqual(result_log.append_u_curve(0.75), "3,0.750000")
        self.assertEqual(self.path.read_text(), "1,0.581635\n2,0.250000\n3,0.750000\n")

    def test_legacy_migration_preserves_original_once(self):
        old = "2026-09-19 21:57:44,0.123456\n2026-09-19 22:00:00,0.654321\n"
        self.seed(old)
        self.assertEqual(result_log.initialize_log(), 2)
        self.assertEqual(self.path.read_text(), "1,0.123456\n2,0.654321\n")
        self.assertEqual(Path(str(self.path) + ".legacy").read_text(), old)
        result_log._state = None
        self.assertEqual(result_log.append_u_curve(0.5), "3,0.500000")
        self.assertEqual(len(list(self.directory.glob("*.legacy*"))), 1)

    def native_rename_os(self, rename=None):
        return SimpleNamespace(stat=os.stat, mkdir=os.mkdir, remove=os.remove,
                               rename=os.rename if rename is None else rename)

    def test_migration_with_firmware_style_no_overwrite_rename(self):
        old = "2026-09-19 21:57:44,0.123456\n"
        self.seed(old)
        with patch.object(result_log, "os", self.native_rename_os()):
            self.assertEqual(result_log.append_u_curve(0.5), "2,0.500000")
        self.assertEqual(self.path.read_text(), "1,0.123456\n2,0.500000\n")
        self.assertEqual(Path(str(self.path) + ".legacy").read_text(), old)
        self.assertFalse(Path(str(self.path) + ".replace_previous").exists())

    def test_interrupted_replacement_finishes_staged_primary(self):
        self.seed("1,0.500000\n")
        self.path.rename(str(self.path) + ".replace_previous")
        Path(str(self.path) + ".pending").write_text("1,0.500000\n2,0.250000\n", encoding="ascii")
        self.assertEqual(result_log.initialize_log(), 2)
        self.assertEqual(self.path.read_text(), "1,0.500000\n2,0.250000\n")
        self.assertEqual(Path(str(self.path) + ".recovered_previous").read_text(), "1,0.500000\n")

    def test_interrupted_replacement_without_staging_restores_original(self):
        self.seed("1,0.500000\n")
        self.path.rename(str(self.path) + ".replace_previous")
        self.assertEqual(result_log.initialize_log(), 1)
        self.assertEqual(self.path.read_text(), "1,0.500000\n")

    def test_replacement_rename_failure_restores_the_original(self):
        old = "2026-09-19 21:57:44,0.123456\n"
        self.seed(old)

        def failed_rename(source, target):
            if source == str(self.path) + ".pending":
                raise OSError("rename failed")
            return os.rename(source, target)

        with patch.object(result_log, "os", self.native_rename_os(failed_rename)):
            with self.assertRaises(OSError):
                result_log.initialize_log()
        self.assertEqual(self.path.read_text(), old)
        self.assertFalse(Path(str(self.path) + ".replace_previous").exists())

    def test_mixed_legacy_and_numbered_records_are_normalized(self):
        self.seed("2026-09-19 21:57:44,0.123456\n2,0.654321\n")
        self.assertEqual(result_log.append_u_curve(0.5), "3,0.500000")
        self.assertEqual(self.path.read_text(), "1,0.123456\n2,0.654321\n3,0.500000\n")

    def test_complete_last_record_without_newline_is_separated(self):
        self.seed("1,0.500000")
        self.assertEqual(result_log.append_u_curve(0.25), "2,0.250000")
        self.assertEqual(self.path.read_text(), "1,0.500000\n2,0.250000\n")

    def test_partial_trailing_record_is_backed_up_and_recovered(self):
        old = "1,0.500000\n2,"
        self.seed(old)
        self.assertEqual(result_log.append_u_curve(0.25), "2,0.250000")
        self.assertEqual(self.path.read_text(), "1,0.500000\n2,0.250000\n")
        self.assertEqual(Path(str(self.path) + ".recovered").read_text(), old)

    def test_corrupt_complete_record_is_reported_without_rewriting(self):
        old = "1,0.500000\nbroken\n3,0.250000\n"
        self.seed(old)
        with self.assertRaisesRegex(ValueError, "invalid SD record 2"):
            result_log.append_u_curve(0.2)
        self.assertEqual(self.path.read_text(), old)

    def test_truncated_numeric_score_is_not_accepted_as_a_complete_value(self):
        old = "1,0.500000\n2,0.123"
        self.seed(old)
        self.assertEqual(result_log.append_u_curve(0.25), "2,0.250000")
        self.assertEqual(self.path.read_text(), "1,0.500000\n2,0.250000\n")
        self.assertEqual(Path(str(self.path) + ".recovered").read_text(), old)

    def test_discontinuous_numbering_is_rejected(self):
        self.seed("1,0.500000\n3,0.250000\n")
        with self.assertRaises(ValueError):
            result_log.initialize_log()

    def test_nonfinite_values_do_not_create_a_file_or_consume_an_index(self):
        for value in (float("nan"), float("inf"), -float("inf")):
            with self.assertRaises(ValueError):
                result_log.append_u_curve(value)
        self.assertFalse(self.path.exists())
        self.assertEqual(result_log.append_u_curve(0.2), "1,0.200000")

    def test_count_is_cached_between_normal_appends(self):
        with patch.object(result_log, "_scan", wraps=result_log._scan) as scan:
            result_log.append_u_curve(0.1)
            result_log.line_count()
            result_log.append_u_curve(0.2)
            result_log.line_count()
            self.assertEqual(scan.call_count, 1)

    def test_external_size_change_triggers_a_rescan(self):
        result_log.append_u_curve(0.1)
        self.path.write_text("1,0.100000\n2,0.200000\n", encoding="ascii")
        self.assertEqual(result_log.append_u_curve(0.3), "3,0.300000")

    def test_large_initialized_log_only_reads_the_new_record_on_append(self):
        self.seed("".join("%d,0.500000\n" % index for index in range(1, 2001)))
        result_log.initialize_log()
        original_open = builtins.open
        reads = []

        class CountedReader:
            def __init__(self, handle):
                self.handle = handle
            def __getattr__(self, name):
                return getattr(self.handle, name)
            def read(self, length=-1):
                reads.append(length)
                if length < 0:
                    raise AssertionError("unbounded log read")
                return self.handle.read(length)
            def readline(self, length=-1):
                raise AssertionError("normal append rescanned the history")

        def counted_open(path, mode):
            handle = original_open(path, mode)
            return CountedReader(handle) if mode == "rb" else handle

        with patch.object(result_log, "open", counted_open, create=True):
            self.assertEqual(result_log.append_u_curve(0.25), "2001,0.250000")
        self.assertEqual(reads, [len(b"2001,0.250000\n")])

    def test_expected_index_mismatch_does_not_write(self):
        result_log.append_u_curve(0.1)
        before = self.path.read_bytes()
        with self.assertRaises(RuntimeError):
            result_log.append_u_curve(0.2, expected_index=1)
        self.assertEqual(self.path.read_bytes(), before)

    def test_permission_error_is_not_treated_as_an_empty_log(self):
        with patch.object(result_log, "_stat", side_effect=PermissionError(13, "denied")):
            with self.assertRaises(PermissionError):
                result_log.line_count()

    def test_existing_file_cannot_be_used_as_a_directory(self):
        self.directory.write_text("not a directory")
        with self.assertRaises(OSError):
            result_log.initialize_log()

    def assert_fault_rolls_back(self, fault):
        self.seed("1,0.100000\n")
        result_log.initialize_log()
        original_open = builtins.open
        injected = [False]

        def fault_open(path, mode):
            handle = original_open(path, mode)
            target_mode = "rb" if fault == "readback" else "ab"
            if str(path) == str(self.path) and mode == target_mode and not injected[0]:
                injected[0] = True
                return FaultFile(handle, fault)
            return handle

        with patch.object(result_log, "open", fault_open, create=True):
            with self.assertRaises(OSError):
                result_log.append_u_curve(0.2)
        self.assertEqual(self.path.read_text(), "1,0.100000\n")
        self.assertTrue(Path(str(self.path) + ".failed").exists())
        self.assertEqual(result_log.append_u_curve(0.3), "2,0.300000")

    def test_short_write_rolls_back(self):
        self.assert_fault_rolls_back("short")

    def test_flush_failure_rolls_back(self):
        self.assert_fault_rolls_back("flush")

    def test_flush_failure_rolls_back_with_firmware_style_rename(self):
        with patch.object(result_log, "os", self.native_rename_os()):
            self.assert_fault_rolls_back("flush")

    def test_close_failure_rolls_back(self):
        self.assert_fault_rolls_back("close")

    def test_readback_mismatch_rolls_back(self):
        self.assert_fault_rolls_back("readback")

    def debug_namespace(self):
        owner = self
        owner.shutdown_count = 0

        class Frame:
            def save(self, path, **kwargs):
                Path(path).write_bytes(b"fake camera frame")

        class Session:
            def __init__(self, **kwargs):
                self.camera = SimpleNamespace(snapshot=Frame, frame_to_gray=lambda frame: object())
                self.stm32 = SimpleNamespace(light_on=lambda: True)
            def initialize(self):
                pass
            def show(self, frame):
                pass
            def shutdown(self):
                owner.shutdown_count += 1
            def _try_light_off(self, context):
                return True

        namespace = {
            "DebugSession": Session, "now_ms": lambda: 0,
            "time": SimpleNamespace(sleep_ms=lambda ms: None),
            "LIGHT_CAPTURE_DELAY_MS": 2000, "LIGHT_ON_DURATION_MS": 5000,
            "_require_live_frame": lambda frame: None,
            "DEBUG_CAPTURE_DIRECTORY": str(Path(self.temporary.name) / "captures"),
            "DEBUG_JPEG_QUALITY": 100, "_capture_indices": {},
            "gc": SimpleNamespace(collect=lambda: None),
            "elapsed_ms": lambda start: 5000, "mem_free": lambda: 100000,
            "evaluate_gray": lambda gray: ({"total_ms": 20, "min_free_bytes": 50000}, 0.5),
            "os": os, "RESULT_TXT": str(self.path),
            "line_count": result_log.line_count, "append_u_curve": result_log.append_u_curve,
            "next_measurement_index": result_log.next_measurement_index,
            "unused_path": result_log.unused_path, "ensure_directory": result_log.ensure_directory,
        }
        return source_objects("debug_main.py", {"_ensure_capture_directory", "_capture_path", "_measurement_path", "capture_evaluate_save"}, namespace)

    def test_debug_photo_uses_the_saved_measurement_index(self):
        result_log.append_u_curve(0.1)
        namespace = self.debug_namespace()
        row = namespace["capture_evaluate_save"]()
        self.assertEqual(row["index"], 2)
        self.assertEqual(Path(row["image_path"]).name, "measure_2.jpg")
        self.assertTrue(Path(row["image_path"]).exists())
        self.assertEqual(row["line"], "2,0.500000")
        self.assertEqual(self.shutdown_count, 1)

    def test_debug_log_failure_removes_promoted_photo(self):
        namespace = self.debug_namespace()
        namespace["append_u_curve"] = lambda *args, **kwargs: (_ for _ in ()).throw(OSError("SD failed"))
        with self.assertRaises(OSError):
            namespace["capture_evaluate_save"]()
        self.assertEqual(list(Path(namespace["DEBUG_CAPTURE_DIRECTORY"]).iterdir()), [])
        self.assertEqual(result_log.line_count(), 0)
        self.assertEqual(self.shutdown_count, 1)

    def test_debug_evaluation_failure_removes_pending_photo(self):
        namespace = self.debug_namespace()
        namespace["evaluate_gray"] = lambda gray: (_ for _ in ()).throw(RuntimeError("bad evaluation"))
        with self.assertRaises(RuntimeError):
            namespace["capture_evaluate_save"]()
        self.assertEqual(list(Path(namespace["DEBUG_CAPTURE_DIRECTORY"]).iterdir()), [])
        self.assertEqual(self.shutdown_count, 1)

    def test_uncommitted_photo_is_archived_before_reusing_its_index(self):
        namespace = self.debug_namespace()
        captures = Path(namespace["DEBUG_CAPTURE_DIRECTORY"])
        captures.mkdir()
        (captures / "measure_1.jpg").write_bytes(b"old uncommitted photo")
        (captures / "measure_1.jpg.pending").write_bytes(b"old pending photo")
        row = namespace["capture_evaluate_save"]()
        self.assertEqual(row["index"], 1)
        self.assertEqual((captures / "measure_1.jpg.orphan").read_bytes(), b"old uncommitted photo")
        self.assertEqual((captures / "measure_1.jpg.pending.orphan").read_bytes(), b"old pending photo")

    def test_empty_camera_image_is_not_committed(self):
        namespace = self.debug_namespace()
        original_session = namespace["DebugSession"]

        class EmptyFrame:
            def save(self, path, **kwargs):
                Path(path).write_bytes(b"")

        def empty_session(**kwargs):
            session = original_session(**kwargs)
            session.camera.snapshot = EmptyFrame
            return session

        namespace["DebugSession"] = empty_session
        with self.assertRaisesRegex(OSError, "empty measurement image"):
            namespace["capture_evaluate_save"]()
        self.assertEqual(result_log.line_count(), 0)
        self.assertEqual(list(Path(namespace["DEBUG_CAPTURE_DIRECTORY"]).iterdir()), [])
        self.assertEqual(self.shutdown_count, 1)

    def test_standalone_capture_numbering_does_not_overwrite(self):
        namespace = self.debug_namespace()
        first = Path(namespace["_capture_path"]("capture"))
        first.write_bytes(b"first")
        second = Path(namespace["_capture_path"]("capture"))
        self.assertEqual(first.name, "capture_1.jpg")
        self.assertEqual(second.name, "capture_2.jpg")
        self.assertEqual(first.read_bytes(), b"first")

    def test_uart_exception_does_not_turn_a_saved_result_into_failure(self):
        result_log.append_u_curve(0.1)
        namespace = {
            "Camera": lambda: SimpleNamespace(capture_gray=lambda: object()),
            "now_ms": lambda: 0, "elapsed_ms": lambda start: 5000,
            "time": SimpleNamespace(sleep_ms=lambda ms: None),
            "LIGHT_CAPTURE_DELAY_MS": 2000, "LIGHT_ON_DURATION_MS": 5000,
            "evaluate_gray": lambda gray: ({"total_ms": 20, "min_free_bytes": 50000}, 0.5),
            "line_count": result_log.line_count, "append_u_curve": result_log.append_u_curve,
            "_is_ide_control_interrupt": lambda error: "IDE interrupt" in str(error),
            "gc": SimpleNamespace(collect=lambda: None), "mem_free": lambda: 100000,
            "RESULT_TXT": str(self.path),
        }
        source_objects("data_touch/app.py", {"CameraApplication"}, namespace)
        application = namespace["CameraApplication"]()
        application.stm32 = SimpleNamespace(
            light_on=lambda: True, light_off=lambda: True,
            send_u_curve=lambda value: (_ for _ in ()).throw(OSError("UART failed")),
        )
        row = application.measure_once(1)
        self.assertEqual(row["index"], 2)
        self.assertFalse(row["uart_ack"])
        self.assertEqual(result_log.line_count(), 2)
        self.assertFalse(application.measurement_busy)

    def restore_code(self):
        source = (DEPLOY_ROOT / "upload_k230_final_deploy.ps1").read_text(encoding="utf-8")
        code = re.search(r"\$restoreCode = @'\n(.*?)\n'@", source, re.S).group(1)
        return compile(code.replace("'/sdcard/data_touch_results/u_curve.txt'", repr(str(self.path))), "restore_code", "exec")

    def test_restore_preserves_previous_log_before_replacing_it(self):
        self.seed("1,0.100000\n")
        staged = Path(str(self.path) + ".restore_pending")
        staged.write_text("1,0.200000\n2,0.300000\n", encoding="ascii")
        native_os = SimpleNamespace(stat=os.stat, rename=os.rename, remove=os.remove)
        with patch.dict(sys.modules, {"os": native_os}):
            exec(self.restore_code(), {})
        self.assertEqual(self.path.read_text(), "1,0.200000\n2,0.300000\n")
        self.assertEqual(Path(str(self.path) + ".before_restore").read_text(), "1,0.100000\n")
        self.assertFalse(staged.exists())

    def test_restore_rename_failure_keeps_primary_log_and_staging(self):
        self.seed("1,0.100000\n")
        staged = Path(str(self.path) + ".restore_pending")
        staged.write_text("1,0.200000\n", encoding="ascii")
        native_os = SimpleNamespace(
            stat=os.stat, remove=os.remove,
            rename=lambda *args: (_ for _ in ()).throw(OSError("rename failed")),
        )
        with patch.dict(sys.modules, {"os": native_os}):
            with self.assertRaises(OSError):
                exec(self.restore_code(), {})
        self.assertEqual(self.path.read_text(), "1,0.100000\n")
        self.assertTrue(staged.exists())


if __name__ == "__main__":
    unittest.main()
