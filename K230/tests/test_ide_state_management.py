import json
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
FIRMWARE_IDE = PROJECT_ROOT / "native" / "canmv_port" / "port" / "omv" / "ide_dbg.c"
FIRMWARE_MAIN = PROJECT_ROOT / "native" / "canmv_port" / "port" / "core" / "main.c"
EXTENSION_ROOT = PROJECT_ROOT / "host_tools" / "canmv_vscode"
BACKEND_MAIN = PROJECT_ROOT / "host_tools" / "canmv_vscode" / "native" / "go" / "cmd" / "canmv-backend" / "main.go"


class IdeStateManagementTests(unittest.TestCase):
    def test_connect_and_disconnect_do_not_request_a_reset(self):
        source = FIRMWARE_IDE.read_text(encoding="utf-8")
        attach = source[source.index("static void ide_dbg_route_protocol_start"):source.index("static void ide_dbg_route_bytes")]
        disconnect = source[source.index("static void ide_dbg_disconnect(void)"):source.index("void ide_dbg_interrupt")]
        self.assertIn("ide_dbg_set_attached(true)", attach)
        self.assertNotIn("request_soft_reset", attach)
        self.assertNotIn("request_debug_takeover", attach)
        self.assertNotIn("request_soft_reset", disconnect)
        self.assertNotIn("request_debug_takeover", disconnect)

    def test_only_explicit_stop_arms_debug_takeover(self):
        source = FIRMWARE_IDE.read_text(encoding="utf-8")
        stop_case = source[source.index("case USBDBG_SCRIPT_STOP: {"):source.index("case USBDBG_SCRIPT_SAVE: {")]
        self.assertIn("ide_dbg_request_debug_takeover()", stop_case)
        takeover = source[source.index("static void ide_dbg_request_debug_takeover"):source.index("static void ide_dbg_disconnect(void)")]
        self.assertIn("ide_dbg_set_auto_exec_suppressed(true)", takeover)
        self.assertIn("ide_dbg_request_soft_reset()", takeover)

    def test_legacy_script_status_includes_autoexec_python(self):
        source = FIRMWARE_IDE.read_text(encoding="utf-8")
        start = source.index("case USBDBG_SCRIPT_RUNNING: {")
        status_case = source[start:source.index("case USBDBG_SCRIPT_STATUS: {", start)]
        self.assertIn("ide_dbg_is_script_running()", status_case)
        self.assertNotIn("ide_dbg_script_running();", status_case)

    def test_autoexec_is_not_conditioned_on_ide_connection(self):
        source = FIRMWARE_MAIN.read_text(encoding="utf-8")
        start = source.index("// Auto-run boot.py/main.py")
        autoexec = source[start:source.index("for (;;) {", start)]
        self.assertIn("ide_dbg_auto_exec_allowed()", autoexec)
        self.assertNotIn("ide_dbg_is_connected()", autoexec)

    def test_extension_disconnect_is_transport_only(self):
        source = (EXTENSION_ROOT / "extension.js").read_text(encoding="utf-8")
        start = source.index("const disconnectBoardRuntime")
        disconnect = source[start:source.index("disposables = [", start)]
        self.assertIn("Disconnect is transport-only", disconnect)
        self.assertNotIn("stopRunningScript", disconnect)

    def test_connect_auto_attaches_preview_without_takeover(self):
        source = (EXTENSION_ROOT / "extension.js").read_text(encoding="utf-8")
        connect = source[source.index("const connectBoardRuntime"):source.index("const disconnectBoardRuntime")]
        self.assertIn("if (connected && scriptRunning) schedulePreviewAuto(150)", connect)
        self.assertNotIn("stopRunningScript", connect)

    def test_backend_connect_is_observational(self):
        source = BACKEND_MAIN.read_text(encoding="utf-8")
        connect = source[source.index("func (s *server) connectBoard"):source.index("func handshakeResponded")]
        observe = source[source.index("func (s *server) scheduleConnectObservation"):source.index("func (s *server) isCurrentBoard")]
        self.assertIn("scheduleConnectObservation", connect)
        self.assertNotIn("scheduleConnectSoftReset", connect)
        self.assertIn("scriptBusy(board, false)", observe)
        self.assertNotIn("ScriptStop", observe)
        self.assertNotIn("softResetBoard", observe)

    def test_backend_disconnect_is_transport_only(self):
        source = BACKEND_MAIN.read_text(encoding="utf-8")
        cleanup = source[source.index("func (s *server) cleanupBoard"):source.index("func (s *server) abortBoard")]
        self.assertIn("Disconnect is transport-only", cleanup)
        self.assertNotIn("stopScriptAndDrain", cleanup)
        self.assertNotIn("softResetBoard", cleanup)

    def test_extension_run_performs_explicit_takeover(self):
        source = (EXTENSION_ROOT / "extension.js").read_text(encoding="utf-8")
        prepare = source[source.index("const ensureCanStartScript"):source.index("const runRemotePathLocked")]
        self.assertIn("Methods.scriptRunning", prepare)
        self.assertIn("stopRunningScriptLocked", prepare)
        self.assertIn("Unable to enter DEBUG_IDLE", source)
        self.assertIn("waitForDebugIdle", source)

    def test_run_commands_remain_enabled_while_production_runs(self):
        package = json.loads((EXTENSION_ROOT / "package.json").read_text(encoding="utf-8"))
        commands = {item["command"]: item for item in package["contributes"]["commands"]}
        for name in ("canmv.runCurrentScript", "canmv.runRemoteFile", "canmv.runExampleFile", "canmv.runOnK230"):
            condition = commands[name]["enablement"]
            self.assertIn("canmv.connected", condition)
            self.assertNotIn("canmv.boardReady", condition)
            self.assertNotIn("!canmv.scriptRunning", condition)


if __name__ == "__main__":
    unittest.main()
