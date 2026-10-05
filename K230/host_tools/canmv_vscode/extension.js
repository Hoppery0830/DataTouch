"use strict";
var __create = Object.create;
var __defProp = Object.defineProperty;
var __getOwnPropDesc = Object.getOwnPropertyDescriptor;
var __getOwnPropNames = Object.getOwnPropertyNames;
var __getProtoOf = Object.getPrototypeOf;
var __hasOwnProp = Object.prototype.hasOwnProperty;
var __export = (target, all) => {
  for (var name in all)
    __defProp(target, name, { get: all[name], enumerable: true });
};
var __copyProps = (to, from, except, desc) => {
  if (from && typeof from === "object" || typeof from === "function") {
    for (let key of __getOwnPropNames(from))
      if (!__hasOwnProp.call(to, key) && key !== except)
        __defProp(to, key, { get: () => from[key], enumerable: !(desc = __getOwnPropDesc(from, key)) || desc.enumerable });
  }
  return to;
};
var __toESM = (mod, isNodeMode, target) => (target = mod != null ? __create(__getProtoOf(mod)) : {}, __copyProps(
  // If the importer is in node compatibility mode or this is not an ESM
  // file that has been converted to a CommonJS file using a Babel-
  // compatible transform (i.e. "__esModule" has not been set), then set
  // "default" to the CommonJS "module.exports" for node compatibility.
  isNodeMode || !mod || !mod.__esModule ? __defProp(target, "default", { value: mod, enumerable: true }) : target,
  mod
));
var __toCommonJS = (mod) => __copyProps(__defProp({}, "__esModule", { value: true }), mod);

// src/extension.ts
var extension_exports = {};
__export(extension_exports, {
  activate: () => activate,
  deactivate: () => deactivate
});
module.exports = __toCommonJS(extension_exports);
var cp5 = __toESM(require("child_process"));
var fs13 = __toESM(require("fs"));
var path14 = __toESM(require("path"));
var vscode24 = __toESM(require("vscode"));

// src/backend/native.ts
var cp = __toESM(require("child_process"));
var fs = __toESM(require("fs"));
var path = __toESM(require("path"));
var vscode2 = __toESM(require("vscode"));

// src/protocol/codec.ts
var JsonCodec = class {
  encodeRequest(req) {
    return JSON.stringify(req);
  }
  decodeMessage(raw) {
    const parsed = JSON.parse(raw);
    if ("result" in parsed) {
      return { id: parsed.id, result: parsed.result };
    }
    if ("error" in parsed) {
      return { id: parsed.id, error: parsed.error };
    }
    if ("event" in parsed) {
      return { event: parsed.event, params: parsed.params ?? {} };
    }
    throw new Error(`Invalid protocol message: ${raw}`);
  }
};

// src/protocol/methods.ts
var Methods = {
  /** Scan host serial devices for supported CanMV boards. */
  detectBoards: {
    method: "detectBoards",
    params: {},
    result: {},
    errors: {}
  },
  /** Open serial connection to the board. */
  connectBoard: {
    method: "connectBoard",
    params: {},
    result: {},
    errors: {
      1001: "Board not found",
      1002: "Connection timeout",
      1003: "Already connected"
    }
  },
  /** Close serial connection. */
  disconnectBoard: {
    method: "disconnectBoard",
    params: {},
    result: {},
    errors: { 1004: "Not connected" }
  },
  /** Execute a Python script on the board. */
  runScript: {
    method: "runScript",
    params: {},
    result: {},
    errors: {
      2001: "Script parse error",
      2002: "Script timeout"
    }
  },
  /** Interrupt the running script (Ctrl+C). */
  stopScript: {
    method: "stopScript",
    params: {},
    result: {},
    errors: {}
  },
  /** Query whether the board is busy running Python or completing a soft reset. */
  scriptRunning: {
    method: "scriptRunning",
    params: {},
    result: {},
    errors: {
      2003: "Board not connected"
    }
  },
  /** Send interactive terminal input to the board REPL. */
  terminalInput: {
    method: "terminalInput",
    params: {},
    result: {},
    errors: {
      2004: "Board not connected",
      2005: "REPL input unsupported"
    }
  },
  /** Query virtual IDE touch state on the board. */
  virtualTouchStatus: {
    method: "virtualTouch.status",
    params: {},
    result: {},
    errors: {}
  },
  /** Inject a virtual IDE touch event. */
  virtualTouchEvent: {
    method: "virtualTouch.event",
    params: {},
    result: {},
    errors: {
      6001: "Board not connected"
    }
  },
  /** Start video preview streaming. */
  startPreview: {
    method: "startPreview",
    params: {},
    result: {},
    errors: {
      3001: "Not connected",
      3002: "Camera not available"
    }
  },
  /** Stop video preview streaming. */
  stopPreview: {
    method: "stopPreview",
    params: {},
    result: {},
    errors: {}
  },
  /** List directory contents on the board. */
  ioListDir: {
    method: "io.listDir",
    params: {},
    result: {},
    errors: { 4001: "Path not found", 4002: "Not a directory", 4008: "File explorer unsupported" }
  },
  /** Query file metadata on the board. */
  ioQueryFileStat: {
    method: "io.queryFileStat",
    params: {},
    result: {},
    errors: {}
  },
  /** Read file content from the board. */
  ioReadFile: {
    method: "io.readFile",
    params: {},
    result: {},
    errors: { 4001: "File not found", 4003: "Read error", 4008: "File read unsupported" }
  },
  /** Write file content to the board (full overwrite). */
  ioWriteFile: {
    method: "io.writeFile",
    params: {},
    result: {},
    errors: { 4001: "File not found", 4003: "Write error" }
  },
  /** Start a chunked file overwrite. */
  ioBeginWriteFile: {
    method: "io.beginWriteFile",
    params: {},
    result: {},
    errors: { 4003: "Write error", 4008: "File write unsupported" }
  },
  /** Write the next chunk of an active file overwrite. */
  ioWriteFileChunk: {
    method: "io.writeFileChunk",
    params: {},
    result: {},
    errors: { 4003: "Write error" }
  },
  /** Flush and verify an active chunked file overwrite. */
  ioFinishWriteFile: {
    method: "io.finishWriteFile",
    params: {},
    result: {},
    errors: { 4003: "Write error" }
  },
  /** End an incomplete chunked write and release device-side transfer state. */
  ioAbortWriteFile: {
    method: "io.abortWriteFile",
    params: {},
    result: {},
    errors: {}
  },
  /** Delete a file on the board. */
  ioDeleteFile: {
    method: "io.deleteFile",
    params: {},
    result: {},
    errors: { 4001: "File not found", 4004: "Delete error" }
  },
  /** Rename a file or directory on the board. */
  ioRenameFile: {
    method: "io.renameFile",
    params: {},
    result: {},
    errors: { 4001: "File not found", 4005: "Rename error" }
  },
  /** Create a directory on the board. */
  ioMkdir: {
    method: "io.mkdir",
    params: {},
    result: {},
    errors: { 4006: "Create directory error" }
  },
  /** Remove a directory on the board, optionally including its contents. */
  ioRmdir: {
    method: "io.rmdir",
    params: {},
    result: {},
    errors: { 4007: "Remove directory error" }
  },
  /** Get board firmware git commit hash (for stub version matching). */
  getFirmwareCommit: {
    method: "getFirmwareCommit",
    params: {},
    result: {},
    errors: { 5001: "Not connected", 5002: "Commit not available" }
  },
  /** Execute a file already on the K230 (fire-and-forget). */
  ioFileExec: {
    method: "io.fileExec",
    params: {},
    result: {},
    errors: { 5001: "Not connected", 5002: "Path error" }
  }
};
var nextId = 1;
function resetRequestId() {
  nextId = 1;
}
function nextRequestId() {
  return nextId++;
}
function createRequest(def, params) {
  return {
    id: nextRequestId(),
    method: def.method,
    params
  };
}

// src/protocol/framed_reader.ts
var MSG_REQUEST = 1;
var MSG_RESPONSE = 2;
var MSG_EVENT = 3;
var MSG_FRAME = 4;
var MAGIC = Buffer.from([67, 77]);
var HEADER_SIZE = 7;
var MAX_FRAME_SIZE = 50 * 1024 * 1024;
var FRAME_TIMEOUT_MS = 5e3;
var FramedMessageReader = class {
  constructor(callbacks) {
    this.buffer = Buffer.alloc(0);
    this.state = "HEADER";
    this.currentType = 0;
    this.currentPayloadLen = 0;
    this.remainingDiscardBytes = 0;
    // remaining to skip in DISCARD state
    this.frameTimer = null;
    this.codec = new JsonCodec();
    this.callbacks = callbacks;
  }
  /** Feed a chunk of raw stdout data. May dispatch zero, one, or multiple messages. */
  handleData(chunk) {
    if (process.env.CANMV_PROFILE === "1") {
      this._chunkTs = performance.now();
    }
    if (this.state === "DISCARD") {
      const consume = Math.min(chunk.length, this.remainingDiscardBytes);
      this.remainingDiscardBytes -= consume;
      if (chunk.length > consume) {
        this.buffer = Buffer.concat([this.buffer, chunk.subarray(consume)]);
      }
      if (this.remainingDiscardBytes <= 0) {
        this.state = "HEADER";
      }
      while (this.tryConsume()) {
      }
      return;
    }
    this.buffer = this.buffer.length === 0 ? chunk : Buffer.concat([this.buffer, chunk]);
    while (this.tryConsume()) {
    }
  }
  /** Reset all state — called on disconnect. */
  reset() {
    this.buffer = Buffer.alloc(0);
    this.state = "HEADER";
    this.currentType = 0;
    this.currentPayloadLen = 0;
    this.remainingDiscardBytes = 0;
    this.clearFrameTimer();
  }
  // ── private ──
  tryConsume() {
    if (this.state === "DISCARD") return false;
    if (this.state === "HEADER") return this.tryReadHeader();
    return this.tryReadPayload();
  }
  tryReadHeader() {
    if (this.buffer.length < HEADER_SIZE) return false;
    if (this.buffer[0] !== MAGIC[0] || this.buffer[1] !== MAGIC[1]) {
      const magicAt = this.buffer.indexOf(MAGIC);
      if (magicAt >= 0) {
        this.buffer = this.buffer.subarray(magicAt);
      } else {
        const keepLast = this.buffer[this.buffer.length - 1] === MAGIC[0];
        this.buffer = keepLast ? this.buffer.subarray(this.buffer.length - 1) : Buffer.alloc(0);
      }
      return this.buffer.length >= HEADER_SIZE;
    }
    this.currentType = this.buffer[2];
    this.currentPayloadLen = this.buffer.readUInt32LE(3);
    this.buffer = this.buffer.subarray(HEADER_SIZE);
    if (this.currentType === MSG_FRAME && this.currentPayloadLen > MAX_FRAME_SIZE) {
      this.remainingDiscardBytes = this.currentPayloadLen;
      this.state = "DISCARD";
      const skip = Math.min(this.buffer.length, this.remainingDiscardBytes);
      this.remainingDiscardBytes -= skip;
      this.buffer = this.buffer.subarray(skip);
      if (this.remainingDiscardBytes <= 0) {
        this.state = "HEADER";
      }
      return this.buffer.length >= HEADER_SIZE;
    }
    if (this.currentType === MSG_FRAME) {
      this.startFrameTimer();
    }
    this.state = "PAYLOAD";
    return this.buffer.length > 0;
  }
  tryReadPayload() {
    if (this.buffer.length < this.currentPayloadLen) return false;
    const payload = this.buffer.subarray(0, this.currentPayloadLen);
    this.buffer = this.buffer.subarray(this.currentPayloadLen);
    this.state = "HEADER";
    if (this.currentType === MSG_FRAME) {
      this.clearFrameTimer();
    }
    this.dispatch(this.currentType, payload);
    return this.buffer.length > 0;
  }
  dispatch(type, payload) {
    if (type === MSG_REQUEST || type === MSG_RESPONSE || type === MSG_EVENT) {
      const raw = payload.toString("utf-8");
      try {
        const msg = this.codec.decodeMessage(raw);
        this.callbacks.onMessage(msg);
      } catch {
      }
    } else if (type === MSG_FRAME) {
      const dispatchTs = performance.now();
      if (payload.length < 8) return;
      const frameId = payload.readUInt32LE(0);
      const jpeg = payload.subarray(4);
      const len = jpeg.length;
      if (len >= 4 && jpeg[0] === 255 && jpeg[1] === 216) {
        let end = len;
        for (let i = len - 2; i >= 2; i--) {
          if (jpeg[i] === 255 && jpeg[i + 1] === 217) {
            end = i + 2;
            break;
          }
        }
        const frame = end === len ? jpeg : jpeg.subarray(0, end);
        const view = new Uint8Array(frame.buffer, frame.byteOffset, frame.byteLength);
        this.callbacks.onFrame(frameId, view, this._chunkTs, dispatchTs);
      }
    }
  }
  startFrameTimer() {
    this.clearFrameTimer();
    this.frameTimer = setTimeout(() => {
      this.buffer = Buffer.alloc(0);
      this.state = "HEADER";
      this.currentPayloadLen = 0;
      this.remainingDiscardBytes = 0;
      this.frameTimer = null;
    }, FRAME_TIMEOUT_MS);
  }
  clearFrameTimer() {
    if (this.frameTimer) {
      clearTimeout(this.frameTimer);
      this.frameTimer = null;
    }
  }
};

// src/protocol/types.ts
function isResponse(msg) {
  return "result" in msg;
}
function isError(msg) {
  return "error" in msg;
}
function isEvent(msg) {
  return "event" in msg;
}
function isFrameEvent(msg) {
  return msg.event === "frameAvailable";
}
var ErrorCodes = {
  // 1000–1999: Connection errors
  CONNECTION: {
    BOARD_NOT_FOUND: 1001,
    TIMEOUT: 1002,
    ALREADY_CONNECTED: 1003,
    NOT_CONNECTED: 1004
  },
  // 2000–2999: Script execution errors
  SCRIPT: {
    PARSE_ERROR: 2001,
    TIMEOUT: 2002
  },
  // 3000–3999: Preview / frame errors
  PREVIEW: {
    NOT_CONNECTED: 3001,
    CAMERA_NOT_AVAILABLE: 3002
  },
  // 9000–9999: Protocol-level errors
  PROTOCOL: {
    INVALID_REQUEST: 9001,
    METHOD_NOT_FOUND: 9002,
    PARSE_ERROR: 9003
  }
};

// src/output.ts
var vscode = __toESM(require("vscode"));
var channel = vscode.window.createOutputChannel("CanMV");
function logDebug(scope, msg) {
  writeLog("DEBUG", scope, msg);
}
function logInfo(scope, msg) {
  writeLog("INFO", scope, msg);
}
function logWarn(scope, msg) {
  writeLog("WARN", scope, msg);
}
function logError(scope, msg) {
  writeLog("ERROR", scope, msg);
}
function logBlock(scope, title, content, maxLines = 80) {
  const lines = content.split(/\r?\n/).filter((line) => line.trim());
  if (lines.length === 0) return;
  writeLog("INFO", scope, `${title} (${lines.length} line${lines.length === 1 ? "" : "s"})`);
  for (const line of lines.slice(0, maxLines)) {
    channel.appendLine(`${timestamp()} [INFO] [${scope}]   ${line}`);
  }
  if (lines.length > maxLines) {
    channel.appendLine(`${timestamp()} [INFO] [${scope}]   ... ${lines.length - maxLines} more lines omitted`);
  }
}
function writeLog(level, scope, msg) {
  const ts = (/* @__PURE__ */ new Date()).toISOString().split("T")[1].split(".")[0];
  const prefix = `[${ts}] [${level}] [${scope}]`;
  const lines = String(msg || "").split(/\r?\n/);
  for (const line of lines) {
    channel.appendLine(`${prefix} ${line}`);
  }
}
function timestamp() {
  return `[${(/* @__PURE__ */ new Date()).toISOString().split("T")[1].split(".")[0]}]`;
}

// src/backend/native.ts
var NativeBackend = class {
  constructor(context) {
    this.context = context;
    this.process = null;
    this._isOpen = false;
    this.closingProcess = null;
    this.pendingRequests = /* @__PURE__ */ new Map();
    this.stderrRemainder = "";
    this._onEvent = new vscode2.EventEmitter();
    this._onDisconnect = new vscode2.EventEmitter();
    this.onEvent = this._onEvent.event;
    this.onDisconnect = this._onDisconnect.event;
    this.codec = new JsonCodec();
    this.reader = new FramedMessageReader({
      onMessage: (msg) => this.handleProtocolMessage(msg),
      onFrame: (frameId, data, chunkTs, dispatchTs) => this._onEvent.fire({
        event: "frameAvailable",
        params: { data, frameId, streamId: "default", chunkTs, dispatchTs }
      })
    });
    const cleanup = () => this.disposeSync();
    context.subscriptions.push({ dispose: cleanup });
    process.once("exit", cleanup);
  }
  async open(_path, _baudRate) {
    if (this._isOpen) return;
    if (this.process) {
      await this.close();
    }
    resetRequestId();
    const backend2 = resolveNativeBackendCommand(this.context);
    logInfo("Backend", `Starting ${backend2.label}: ${backend2.command}${backend2.args.length ? " " + backend2.args.join(" ") : ""}`);
    this.process = cp.spawn(backend2.command, backend2.args, {
      cwd: backend2.cwd,
      stdio: ["pipe", "pipe", "pipe"],
      env: { ...process.env },
      detached: process.platform !== "win32"
    });
    const child = this.process;
    child.unref();
    child.on("error", (err) => {
      logError("Backend", `Spawn error: ${err.message}`);
      if (this.process === child) {
        this._isOpen = false;
      }
    });
    child.on("exit", (code, signal) => {
      this.flushBackendStderr();
      if (this.process !== child) {
        logDebug("Backend", `Previous backend exited (code=${code ?? "null"}, signal=${signal ?? "null"})`);
        return;
      }
      const expectedClose = this.closingProcess === child;
      if (expectedClose) {
        logInfo("Backend", `${backend2.label} stopped (code=${code ?? "null"}, signal=${signal ?? "null"})`);
      } else {
        logWarn("Backend", `${backend2.label} exited (code=${code ?? "null"}, signal=${signal ?? "null"})`);
      }
      this._isOpen = false;
      this.process = null;
      if (this.closingProcess === child) {
        this.closingProcess = null;
      }
      if (!expectedClose) {
        this._onDisconnect.fire();
        this._onEvent.fire({ event: "boardDisconnected", params: {} });
      }
      for (const [, pending] of this.pendingRequests) {
        pending.resolve({ id: 0, error: { code: 1004, message: "Connection lost" } });
      }
      this.pendingRequests.clear();
    });
    child.stdout?.on("data", (chunk) => {
      this.reader.handleData(chunk);
    });
    child.stderr?.on("data", (chunk) => {
      this.handleBackendStderr(chunk);
    });
    this._isOpen = true;
    logDebug("Backend", "Backend process is ready");
  }
  async close() {
    const child = this.process;
    if (child) {
      logInfo("Backend", "Stopping backend");
      this.closingProcess = child;
      this._isOpen = false;
      this.reader.reset();
      this.resolvePendingRequests("Not connected");
      if (child.exitCode === null && child.signalCode === null) {
        signalChildProcess(child, "SIGTERM");
        const terminated = await waitForExit(child, 1e3);
        if (!terminated) {
          logWarn("Backend", "Backend did not exit after abort; killing");
          signalChildProcess(child, "SIGKILL");
          await waitForExit(child, 500);
        }
      }
      if (this.process === child) {
        this.process = null;
      }
      if (this.closingProcess === child) {
        this.closingProcess = null;
      }
    }
    this._isOpen = false;
    this.reader.reset();
    this.resolvePendingRequests("Not connected");
  }
  disposeSync() {
    const child = this.process;
    if (!child) {
      return;
    }
    this.closingProcess = child;
    try {
      child.stdin?.destroy();
      child.stdout?.destroy();
      child.stderr?.destroy();
    } catch {
    }
    if (child.exitCode === null && child.signalCode === null) {
      signalChildProcess(child, "SIGTERM");
      const killTimer = setTimeout(() => {
        if (child.exitCode === null && child.signalCode === null) {
          signalChildProcess(child, "SIGKILL");
        }
      }, 3e3);
      killTimer.unref();
    }
    this.process = null;
    this._isOpen = false;
    this.reader.reset();
    for (const [, pending] of this.pendingRequests) {
      pending.resolve({ id: 0, error: { code: 1004, message: "Not connected" } });
    }
    this.pendingRequests.clear();
  }
  isOpen() {
    return this._isOpen;
  }
  async request(req) {
    return new Promise((resolve3) => {
      const wire = this.codec.encodeRequest(req);
      this.pendingRequests.set(req.id, { method: req.method, startedAt: Date.now(), resolve: resolve3 });
      if (!this._isOpen || !this.process?.stdin?.writable) {
        logError("Backend", `Cannot send ${req.method}: backend stdin is not writable`);
        resolve3({ id: req.id, error: { code: 1004, message: "Backend stdin not available" } });
        this.pendingRequests.delete(req.id);
        return;
      }
      const payload = Buffer.from(wire, "utf-8");
      const header = Buffer.alloc(7);
      header[0] = MAGIC[0];
      header[1] = MAGIC[1];
      header[2] = MSG_REQUEST;
      header.writeUInt32LE(payload.length, 3);
      this.process.stdin.write(header);
      this.process.stdin.write(payload);
    });
  }
  notify(req) {
    if (!this._isOpen || !this.process?.stdin?.writable) {
      logError("Backend", `Cannot send ${req.method}: backend stdin is not writable`);
      return;
    }
    const payload = Buffer.from(this.codec.encodeRequest({ ...req, id: 0 }), "utf-8");
    const header = Buffer.alloc(7);
    header[0] = MAGIC[0];
    header[1] = MAGIC[1];
    header[2] = MSG_REQUEST;
    header.writeUInt32LE(payload.length, 3);
    this.process.stdin.write(header);
    this.process.stdin.write(payload);
  }
  // ── Protocol message dispatch (replaces old processJsonLines / processPendingFrame) ──
  handleProtocolMessage(msg) {
    if (isResponse(msg) || isError(msg)) {
      const pending = this.pendingRequests.get(msg.id);
      if (pending) {
        const elapsed = Date.now() - pending.startedAt;
        if (isError(msg)) {
          logError("Backend", `${pending.method} failed after ${elapsed}ms: ${msg.error.message}`);
        }
        pending.resolve(msg);
        this.pendingRequests.delete(msg.id);
      }
    } else if (isEvent(msg)) {
      this._onEvent.fire(msg);
    }
  }
  handleBackendStderr(chunk) {
    this.stderrRemainder += chunk.toString("utf8");
    const lines = this.stderrRemainder.split(/\r?\n/);
    this.stderrRemainder = lines.pop() || "";
    for (const line of lines) {
      const trimmed = line.trimEnd();
      if (trimmed) {
        logDebug("Backend", trimmed);
      }
    }
  }
  flushBackendStderr() {
    const line = this.stderrRemainder.trim();
    if (line) {
      logDebug("Backend", line);
    }
    this.stderrRemainder = "";
  }
  resolvePendingRequests(message) {
    for (const [, pending] of this.pendingRequests) {
      pending.resolve({ id: 0, error: { code: 1004, message } });
    }
    this.pendingRequests.clear();
  }
};
function waitForExit(child, timeoutMs) {
  return new Promise((resolve3) => {
    if (child.exitCode !== null || child.signalCode !== null) {
      resolve3(true);
      return;
    }
    const timer = setTimeout(() => {
      child.off("exit", onExit);
      resolve3(false);
    }, timeoutMs);
    const onExit = () => {
      clearTimeout(timer);
      resolve3(true);
    };
    child.once("exit", onExit);
  });
}
function signalChildProcess(child, signal) {
  if (process.platform !== "win32" && child.pid) {
    try {
      process.kill(-child.pid, signal);
      return;
    } catch {
    }
  }
  child.kill(signal);
}
function executableName() {
  return process.platform === "win32" ? "canmv-backend.exe" : "canmv-backend";
}
function platformTarget() {
  const arch = process.arch === "x64" ? "x64" : process.arch;
  if (process.platform === "win32") return `win32-${arch}`;
  if (process.platform === "darwin") return `darwin-${arch}`;
  if (process.platform === "linux") return `linux-${arch}`;
  return `${process.platform}-${arch}`;
}
function resolveNativeBackendCommand(context) {
  const packaged = path.join(context.extensionPath, "bin", platformTarget(), executableName());
  if (!fs.existsSync(packaged)) {
    throw new Error(`CanMV Go backend executable not found for ${platformTarget()}. Install a platform-specific extension package.`);
  }
  return {
    label: "Go backend",
    command: packaged,
    args: [],
    cwd: path.dirname(packaged)
  };
}

// src/session/session.ts
var vscode3 = __toESM(require("vscode"));
var Session = class {
  constructor(backend2, options) {
    this.backend = backend2;
    this._state = "disconnected";
    this.reconnectTimer = null;
    this.reconnectAttempt = 0;
    this.lastPath = "";
    this.lastBaudRate = 115200;
    this._onStateChange = new vscode3.EventEmitter();
    this.onStateChange = this._onStateChange.event;
    // ── Auto-reconnect ──
    this.onBackendDisconnect = () => {
      if (!this.autoReconnect) {
        logWarn("Session", "Backend disconnected; auto-reconnect disabled");
        this.transition("disconnected");
        return;
      }
      logWarn("Session", "Backend disconnected; scheduling reconnect");
      this.scheduleReconnect();
    };
    this.autoReconnect = options?.autoReconnect ?? true;
    this.connectionTimeout = options?.connectionTimeout ?? 1e4;
    this.requestTimeout = options?.requestTimeout ?? 1e4;
    this.backendDisconnectSubscription = this.backend.onDisconnect?.(this.onBackendDisconnect);
  }
  get state() {
    return this._state;
  }
  dispose() {
    this.clearReconnectTimer();
    this.backendDisconnectSubscription?.dispose();
    this._onStateChange.dispose();
  }
  // ── Connection Lifecycle ──
  async connect(path15, baudRate) {
    if (this._state === "connecting" || this._state === "connected" || this._state === "streaming") {
      return;
    }
    logInfo("Session", `Connect requested: port=${path15}, baud=${baudRate}`);
    this.lastPath = path15;
    this.lastBaudRate = baudRate;
    this.reconnectAttempt = 0;
    this.clearReconnectTimer();
    this.transition("connecting");
    try {
      await this.withTimeout(this.backend.open(path15, baudRate), this.connectionTimeout);
      this.transition("connected");
    } catch (err) {
      this.transition("disconnected");
      logError("Session", `Connect failed: ${err instanceof Error ? err.message : String(err)}`);
      throw err;
    }
  }
  async disconnect() {
    this.clearReconnectTimer();
    this.reconnectAttempt = 0;
    if (this._state === "disconnected") return;
    try {
      logInfo("Session", "Disconnect requested");
      await this.backend.close();
    } finally {
      this.transition("disconnected");
    }
  }
  startStreaming() {
    if (this._state !== "connected") {
      throw new Error(`Cannot start streaming: session is ${this._state}`);
    }
    this.transition("streaming");
  }
  stopStreaming() {
    if (this._state !== "streaming") return;
    this.transition("connected");
  }
  // ── Request Proxy (with timeout enforcement) ──
  /**
   * Send a protocol Request with timeout enforcement.
   * Session layer owns timeout — BackendApi has no built-in timeout.
   */
  async request(req, options = {}) {
    const timeoutMs = options.timeoutMs ?? this.requestTimeout;
    try {
      return await this.withTimeout(
        this.backend.request(req),
        timeoutMs
      );
    } catch {
      logWarn("Session", `Request timed out: ${req.method} after ${timeoutMs}ms`);
      return {
        id: req.id,
        error: {
          code: ErrorCodes.CONNECTION.TIMEOUT,
          message: `Request '${req.method}' timed out after ${timeoutMs}ms`
        }
      };
    }
  }
  scheduleReconnect() {
    const delays = [1e3, 2e3, 4e3, 8e3, 16e3, 3e4];
    const delay = delays[Math.min(this.reconnectAttempt, delays.length - 1)];
    this.reconnectAttempt++;
    logInfo("Session", `Reconnect attempt ${this.reconnectAttempt} in ${delay}ms`);
    this.transition("connecting");
    this.reconnectTimer = setTimeout(async () => {
      try {
        await this.withTimeout(this.backend.open(this.lastPath, this.lastBaudRate), this.connectionTimeout);
        this.reconnectAttempt = 0;
        this.transition("connected");
      } catch (err) {
        logWarn("Session", `Reconnect failed: ${err instanceof Error ? err.message : String(err)}`);
        this.scheduleReconnect();
      }
    }, delay);
  }
  clearReconnectTimer() {
    if (this.reconnectTimer) {
      clearTimeout(this.reconnectTimer);
      this.reconnectTimer = null;
    }
  }
  transition(newState) {
    if (this._state === newState) return;
    logInfo("Session", `State: ${this._state} -> ${newState}`);
    this._state = newState;
    this._onStateChange.fire(newState);
  }
  withTimeout(promise, ms) {
    return new Promise((resolve3, reject) => {
      const timer = setTimeout(() => reject(new Error(`Operation timed out after ${ms}ms`)), ms);
      promise.then(
        (value) => {
          clearTimeout(timer);
          resolve3(value);
        },
        (err) => {
          clearTimeout(timer);
          reject(err);
        }
      );
    });
  }
};

// src/webview/PreviewPanel.ts
var vscode6 = __toESM(require("vscode"));

// src/webview/BaseToolPanel.ts
var vscode5 = __toESM(require("vscode"));
var path2 = __toESM(require("path"));
var fs2 = __toESM(require("fs"));

// src/i18n.ts
var vscode4 = __toESM(require("vscode"));
var t = vscode4.l10n.t;
var states = {
  disconnected: () => t("Disconnected"),
  connecting: () => t("Connecting..."),
  disconnecting: () => t("Disconnecting..."),
  preparing: () => t("Preparing..."),
  connected: () => t("Connected"),
  streaming: () => t("Streaming"),
  offline: () => t("Offline"),
  ready: () => t("Ready"),
  running: () => t("Running"),
  canmvBoard: () => t("CanMV Board")
};
function injectWebviewStrings(html) {
  const script = `<script>window.__CANMV_L10N__=${jsonForScript(webviewStrings())};</script>`;
  return html.includes("</head>") ? html.replace("</head>", `${script}
</head>`) : `${script}
${html}`;
}
function webviewStrings() {
  return {
    stateDisconnected: states.disconnected(),
    stateConnecting: states.connecting(),
    stateDisconnecting: states.disconnecting(),
    statePreparing: states.preparing(),
    stateConnected: states.connected(),
    stateStreaming: states.streaming(),
    stateOffline: states.offline(),
    stateReady: states.ready(),
    stateRunning: states.running(),
    canmvBoard: states.canmvBoard(),
    terminalTitle: t("CanMV Terminal"),
    monitoring: t("Monitoring"),
    viewMode: t("View Mode"),
    text: t("Text"),
    source: t("Source"),
    saveLog: t("Save Log"),
    clear: t("Clear"),
    clearOutput: t("Clear Output"),
    connectReplInput: t("Connect board to use REPL input"),
    replInputUnavailable: t("REPL input unavailable"),
    previewTitle: t("CanMV Preview"),
    imageEmpty: t("Image: --"),
    imageDecodeError: t("Image: decode error"),
    fpsEmpty: t("FPS: --"),
    fpsLabel: t("FPS: {value}", { value: "{value}" }),
    fpsWithFrameCount: t("FPS: {value} (fc:{frameCount})", { value: "{value}", frameCount: "{frameCount}" }),
    disablePreview: t("Disable Preview"),
    enablePreview: t("Enable Preview"),
    showOriginalSize: t("Show Original Size"),
    fitToWindow: t("Fit to Window"),
    rotateFrame: t("Rotate Frame"),
    saveImage: t("Save Image"),
    pickPixelValue: t("Pick Pixel Value"),
    selectHistogramRoi: t("Select Histogram ROI"),
    recordVideo: t("Record Video"),
    stopRecording: t("Stop Recording"),
    histogram: t("Histogram"),
    histogramColorSpace: t("Histogram color space"),
    rgbColorSpace: t("RGB Color Space"),
    grayscaleColorSpace: t("Grayscale Color Space"),
    labColorSpace: t("LAB Color Space"),
    yuvColorSpace: t("YUV Color Space"),
    none: t("None"),
    fit: t("Fit"),
    histogramTooltip: t("Count {count} ({label} {position})", { count: "{count}", label: "{label}", position: "{position}" }),
    pixelReadout: t("X:{x} Y:{y} RGB({r},{g},{b})", { x: "{x}", y: "{y}", r: "{r}", g: "{g}", b: "{b}" }),
    roiReadout: t("ROI x:{x} y:{y} w:{w} h:{h}", { x: "{x}", y: "{y}", w: "{w}", h: "{h}" }),
    recordingUnavailable: t("Video recording is unavailable in this webview"),
    recordingFailed: t("Recording failed"),
    recordingStarted: t("Recording {format}", { format: "{format}" }),
    recordingSaved: t("Recording ready ({format})", { format: "{format}" }),
    statMean: t("Mean"),
    statMedian: t("Median"),
    statMode: t("Mode"),
    statStdev: t("StDev"),
    statMin: t("Min"),
    statMax: t("Max"),
    statLq: t("LQ"),
    statUq: t("UQ"),
    thresholdEditorTitle: t("Threshold Editor"),
    openImageFile: t("Open Image File"),
    open: t("Open"),
    frameBuffer: t("Frame Buffer"),
    loadLatestPreviewFrame: t("Load latest Preview frame"),
    reset: t("Reset"),
    resetSliders: t("Reset sliders"),
    noImageLoaded: t("No image loaded"),
    sourceImage: t("Source Image"),
    binaryImage: t("Binary Image (white pixels are tracked pixels)"),
    thresholdColorSpace: t("Threshold color space"),
    grayscale: t("Grayscale"),
    lab: t("LAB"),
    invert: t("Invert"),
    grayscaleMin: t("Grayscale Min"),
    grayscaleMax: t("Grayscale Max"),
    lMin: t("L Min"),
    lMax: t("L Max"),
    aMin: t("A Min"),
    aMax: t("A Max"),
    bMin: t("B Min"),
    bMax: t("B Max"),
    grayscaleThreshold: t("Grayscale Threshold"),
    labThreshold: t("LAB Threshold"),
    copy: t("Copy"),
    apply: t("Apply"),
    copyThresholdTuple: t("Copy threshold tuple"),
    replaceSelectedThresholdTuple: t("Replace selected threshold tuple"),
    dropImageFile: t("Drop an image file to load it"),
    previewFrame: t("Preview Frame"),
    noPreviewFrameAvailable: t("No preview frame available"),
    copiedThreshold: t("Copied threshold"),
    updatedSelectedTuple: t("Updated selected tuple"),
    image: t("Image"),
    unableToLoadImage: t("Unable to load image"),
    invalidPpmImage: t("Invalid PPM image")
  };
}
function jsonForScript(value) {
  return JSON.stringify(value).replace(/</g, "\\u003c").replace(/>/g, "\\u003e").replace(/&/g, "\\u0026").replace(/\u2028/g, "\\u2028").replace(/\u2029/g, "\\u2029");
}

// src/webview/BaseToolPanel.ts
var BaseToolPanel = class _BaseToolPanel {
  constructor(id, title, context, htmlFile) {
    this.id = id;
    this.title = title;
    this._disposed = false;
    this.panel = vscode5.window.createWebviewPanel(
      id,
      title,
      _BaseToolPanel.resolveColumn(),
      { enableScripts: true, retainContextWhenHidden: true }
    );
    const htmlPath = path2.join(context.extensionPath, "webview", htmlFile);
    this.panel.webview.html = injectWebviewStrings(fs2.readFileSync(htmlPath, "utf-8"));
    this.onDidDispose = this.panel.onDidDispose;
    this.panel.onDidDispose(() => {
      this._disposed = true;
    });
  }
  static resolveColumn() {
    const groups = /* @__PURE__ */ new Set();
    for (const editor of vscode5.window.visibleTextEditors) {
      const col = editor.viewColumn ?? vscode5.ViewColumn.One;
      groups.add(col);
    }
    return groups.size >= 2 ? Math.max(...groups) : vscode5.ViewColumn.Beside;
  }
  get disposed() {
    return this._disposed;
  }
  reveal() {
    this.panel.reveal(void 0, false);
  }
  dispose() {
    this._disposed = true;
    this.panel.dispose();
  }
  postMessage(message) {
    this.panel.webview.postMessage(message);
  }
  sendState(state) {
    this.postMessage({ type: "state", state });
  }
};

// src/webview/PreviewPanel.ts
var PreviewPanel = class extends BaseToolPanel {
  constructor(context) {
    super("canmvPreview", t("CanMV Preview"), context, "index.html");
    this._onProfile = new vscode6.EventEmitter();
    this._onCommand = new vscode6.EventEmitter();
    this._onSaveImage = new vscode6.EventEmitter();
    this._onSaveVideo = new vscode6.EventEmitter();
    this._onVirtualTouch = new vscode6.EventEmitter();
    this.captureWaiters = [];
    this.onProfile = this._onProfile.event;
    this.onCommand = this._onCommand.event;
    this.onSaveImage = this._onSaveImage.event;
    this.onSaveVideo = this._onSaveVideo.event;
    this.onVirtualTouch = this._onVirtualTouch.event;
    this.panel.webview.onDidReceiveMessage((msg) => {
      if (msg.type === "profile") {
        this._onProfile.fire(msg);
      } else if (msg.type === "previewCommand" && typeof msg.command === "string") {
        this._onCommand.fire(msg.command);
      } else if (msg.type === "saveImage" && msg.data) {
        this._onSaveImage.fire(new Uint8Array(msg.data));
      } else if (msg.type === "saveVideo" && msg.data) {
        const extension = typeof msg.extension === "string" && msg.extension ? msg.extension : "webm";
        const mimeType = typeof msg.mimeType === "string" ? msg.mimeType : "";
        this._onSaveVideo.fire({ data: new Uint8Array(msg.data), extension, mimeType });
      } else if (msg.type === "captureImage") {
        this.resolveCaptureWaiter(msg.data ? new Uint8Array(msg.data) : void 0);
      } else if (msg.type === "virtualTouch") {
        const payload = {
          x: Number(msg.x),
          y: Number(msg.y),
          sourceWidth: Number(msg.sourceWidth),
          sourceHeight: Number(msg.sourceHeight)
        };
        if ([payload.x, payload.y, payload.sourceWidth, payload.sourceHeight].every(Number.isFinite)) {
          this._onVirtualTouch.fire(payload);
        }
      }
    });
    this.panel.onDidDispose(() => {
      while (this.captureWaiters.length) {
        this.captureWaiters.shift()?.(void 0);
      }
    });
    this.postMessage({ type: "profileConfig", enabled: process.env.CANMV_PROFILE === "1" });
  }
  captureImage(timeoutMs = 1200) {
    if (this.disposed) {
      return Promise.resolve(void 0);
    }
    return new Promise((resolve3) => {
      let done = false;
      const finish = (data) => {
        if (done) return;
        done = true;
        clearTimeout(timer);
        const index = this.captureWaiters.indexOf(finish);
        if (index >= 0) this.captureWaiters.splice(index, 1);
        resolve3(data);
      };
      const timer = setTimeout(() => finish(void 0), timeoutMs);
      this.captureWaiters.push(finish);
      this.postMessage({ type: "captureImage" });
    });
  }
  resolveCaptureWaiter(data) {
    const waiter = this.captureWaiters.shift();
    waiter?.(data);
  }
  sendBoardInfo(info) {
    this.postMessage({ type: "boardInfo", ...info });
  }
  sendFrame(frameId, data) {
    this.postMessage({ type: "frame", data, byteLength: data.byteLength, frameId });
  }
  sendStarted() {
    this.postMessage({ type: "started" });
  }
  sendStopped() {
    this.postMessage({ type: "stopped" });
  }
  sendPreviewDisabled(disabled) {
    this.postMessage({ type: "previewDisabled", disabled });
  }
  sendScriptRunning(running) {
    this.postMessage({ type: "scriptRunning", running });
  }
  sendVirtualTouchState(state) {
    this.postMessage({ type: "virtualTouchState", ...state });
  }
  isActive() {
    return this.panel.visible;
  }
};

// src/webview/TerminalViewProvider.ts
var fs3 = __toESM(require("fs"));
var path3 = __toESM(require("path"));
var vscode7 = __toESM(require("vscode"));
var TerminalViewProvider = class {
  constructor(context, scrollback) {
    this.context = context;
    this.scrollback = scrollback;
    this.inputEnabled = false;
    this.inputReason = t("Connect board to use REPL input");
    this.interruptEnabled = false;
    this.pendingText = "";
    this._onClear = new vscode7.EventEmitter();
    this._onInput = new vscode7.EventEmitter();
    this.onClear = this._onClear.event;
    this.onInput = this._onInput.event;
    this.flushDelayMs = 100;
    this.immediateFlushBytes = 256 * 1024;
  }
  resolveWebviewView(webviewView) {
    this.view = webviewView;
    webviewView.webview.options = { enableScripts: true };
    const htmlPath = path3.join(this.context.extensionPath, "webview", "terminal.html");
    webviewView.webview.html = injectWebviewStrings(fs3.readFileSync(htmlPath, "utf-8"));
    webviewView.webview.onDidReceiveMessage((msg) => {
      if (msg.type === "clearTerminal") {
        this._onClear.fire();
        this.clear();
      } else if (msg.type === "saveTerminalLog") {
        void this.saveLog();
      } else if (msg.type === "terminalReady") {
        this.flushPendingText();
        this.postInputState();
      } else if (msg.type === "terminalInput" && typeof msg.text === "string") {
        this._onInput.fire(msg.text);
      }
    });
    webviewView.onDidDispose(() => {
      if (this.view === webviewView) {
        this.view = void 0;
        this.clearPendingText();
      }
    });
    const text = this.scrollback();
    if (text) {
      queueMicrotask(() => this.appendText(text));
    }
    setTimeout(() => this.postInputState(), 50);
  }
  appendText(text) {
    if (!this.view || !text) {
      return;
    }
    this.pendingText += text;
    if (this.pendingText.length >= this.immediateFlushBytes) {
      this.flushPendingText();
      return;
    }
    this.scheduleFlush();
  }
  clear() {
    this.clearPendingText();
    if (this.view) {
      void this.view.webview.postMessage({ type: "clear" });
    }
  }
  scheduleFlush() {
    if (this.flushTimer) {
      return;
    }
    this.flushTimer = setTimeout(() => {
      this.flushTimer = void 0;
      this.flushPendingText();
    }, this.flushDelayMs);
  }
  flushPendingText() {
    if (this.flushTimer) {
      clearTimeout(this.flushTimer);
      this.flushTimer = void 0;
    }
    if (!this.view || !this.pendingText) {
      return;
    }
    const text = this.pendingText;
    this.pendingText = "";
    void this.view.webview.postMessage({ type: "append", text });
  }
  clearPendingText() {
    if (this.flushTimer) {
      clearTimeout(this.flushTimer);
      this.flushTimer = void 0;
    }
    this.pendingText = "";
  }
  async saveLog() {
    const text = this.scrollback();
    if (!text) {
      void vscode7.window.showInformationMessage(t("CanMV: Terminal log is empty."));
      return;
    }
    const stamp = (/* @__PURE__ */ new Date()).toISOString().replace(/[:.]/g, "-");
    const base = vscode7.workspace.workspaceFolders?.[0]?.uri;
    const defaultUri = base ? vscode7.Uri.joinPath(base, `canmv-terminal-${stamp}.log`) : void 0;
    const target = await vscode7.window.showSaveDialog({
      defaultUri,
      filters: { [t("Log File")]: ["log"], [t("Text File")]: ["txt"], [t("All Files")]: ["*"] },
      saveLabel: t("Save Log")
    });
    if (!target) return;
    await vscode7.workspace.fs.writeFile(target, Buffer.from(text, "utf8"));
    void vscode7.window.showInformationMessage(t("CanMV: Terminal log saved to {path}", { path: target.fsPath }));
  }
  setInputEnabled(enabled, reason = "", interruptEnabled = false) {
    if (this.inputEnabled === enabled && this.inputReason === reason && this.interruptEnabled === interruptEnabled) {
      return;
    }
    this.inputEnabled = enabled;
    this.inputReason = reason;
    this.interruptEnabled = interruptEnabled;
    this.postInputState();
  }
  postInputState() {
    if (this.view) {
      void this.view.webview.postMessage({
        type: "inputState",
        enabled: this.inputEnabled,
        reason: this.inputReason,
        interruptEnabled: this.interruptEnabled
      });
    }
  }
};

// src/service/boardService.ts
var vscode8 = __toESM(require("vscode"));
var BoardService = class {
  constructor(session, detector) {
    this.session = session;
    this.detector = detector;
    this.cachedInfo = null;
  }
  async connectBoard(options = {}) {
    const config = vscode8.workspace.getConfiguration("canmv");
    const baudRate = options.baudRate ?? config.get("baudRate", 12e6);
    const interactive = options.interactive !== false;
    const notify = options.notify !== false;
    let port = options.port || "";
    try {
      await this.session.connect("__detect__", baudRate);
      if (!port) {
        const boards = await this.detector.scan();
        logInfo("Board", `Auto-detected ${boards.length} CanMV device${boards.length === 1 ? "" : "s"}`);
        if (boards.length === 0) {
          await this.session.disconnect();
          if (notify) {
            vscode8.window.showErrorMessage(
              t("CanMV: No CanMV device detected. Connect the board via USB and try again.")
            );
          }
          return null;
        }
        if (boards.length === 1 || !interactive) {
          port = boards[0].port;
          logInfo("Board", `Selected device: ${port} (${boards[0].name})`);
        } else {
          const selected = await vscode8.window.showQuickPick(
            boards.map((b) => ({
              label: b.port,
              description: b.name,
              detail: [
                b.vid && b.pid ? "USB " + b.vid + ":" + b.pid : void 0,
                b.serialNumber ? t("Serial {serialNumber}", { serialNumber: b.serialNumber }) : void 0,
                b.description
              ].filter(Boolean).join(" | ")
            })),
            { placeHolder: t("Select CanMV device") }
          );
          if (!selected) {
            await this.session.disconnect();
            return null;
          }
          port = selected.label;
          logInfo("Board", `Selected device: ${port}`);
        }
      }
      const req = createRequest(Methods.connectBoard, { port, baudRate });
      const result = await this.session.request(req);
      if (isResponse(result)) {
        const info = result.result;
        this.cachedInfo = info;
        const connectedPort = info.port || port;
        logInfo(
          "Board",
          `Connected: ${[info.boardName || info.boardType, info.fwVersion, info.memorySize].filter(Boolean).join(" ")} on ${connectedPort}`
        );
        if (info.archStr) {
          logInfo("Board", `ARCH_STR: ${info.archStr}`);
        }
        if (info.repl) {
          logBlock("REPL", "Boot output", redactFirmwareRevision(info.repl), 80);
        }
        if (notify) {
          vscode8.window.showInformationMessage(
            t("CanMV: Connected - {boardType} (FW {firmwareVersion})", { boardType: info.boardType, firmwareVersion: info.fwVersion })
          );
        }
        return info.repl || null;
      } else {
        const err = result;
        logError("Board", `Connect failed: ${err.error.message}`);
        if (notify) vscode8.window.showErrorMessage(t("CanMV: {message}", { message: err.error.message }));
        await this.session.disconnect();
        return null;
      }
    } catch (err) {
      logError("Board", `Connect failed: ${err instanceof Error ? err.message : String(err)}`);
      if (notify) vscode8.window.showErrorMessage(t("CanMV: Failed to connect - {message}", { message: String(err) }));
      await this.session.disconnect();
      return null;
    }
  }
  async disconnectBoard() {
    await this.session.disconnect();
    logInfo("Board", "Disconnected");
    this.cachedInfo = null;
  }
  boardInfo() {
    return this.cachedInfo;
  }
  setBoardInfo(info) {
    this.cachedInfo = info;
  }
};
function redactFirmwareRevision(text) {
  return text.replace(/-([0-9]+)-g[0-9a-fA-F]{7,40}\b/g, "-$1").replace(/-(?:g)?[0-9a-fA-F]{7,40}\b/g, "").replace(/\b[0-9a-fA-F]{40}\b/g, "<revision>");
}

// src/service/scriptService.ts
var vscode9 = __toESM(require("vscode"));
var ScriptService = class {
  constructor(requester) {
    this.requester = requester;
    vscode9.window.onDidChangeActiveTextEditor((editor) => {
      if (editor && editor.document.languageId === "python") {
        this.lastPythonEditor = editor;
      }
    });
    const current = vscode9.window.activeTextEditor;
    if (current && current.document.languageId === "python") {
      this.lastPythonEditor = current;
    }
  }
  async runCurrentScript() {
    let editor = vscode9.window.activeTextEditor;
    if (!editor || editor.document.languageId !== "python") {
      editor = this.lastPythonEditor;
    }
    if (!editor) {
      vscode9.window.showWarningMessage(t("CanMV: No Python file open. Open a .py file first."));
      return false;
    }
    const filename = editor.document.fileName.split("/").pop() || "script.py";
    const script = editor.document.getText();
    return this.runScriptContent(script, filename);
  }
  async runScriptContent(script, filename = "script.py") {
    logInfo("Script", `Run script: ${filename} (${script.length}B)`);
    const req = createRequest(Methods.runScript, { script });
    const result = await this.requester.request(req);
    if (isResponse(result)) {
      const r = result.result;
      if (r.status === "ok") {
        logInfo("Script", `Started: ${filename}`);
        if (r.output) {
          logBlock("REPL", `Output from ${filename}`, r.output, 120);
        }
        vscode9.window.showInformationMessage(t("CanMV: Script executed successfully ({filename}).", { filename }));
        return true;
      } else {
        const message = r.message || r.output || "unknown";
        logWarn("Script", `Run error: ${message}`);
        vscode9.window.showWarningMessage(t("CanMV: Script error - {message}", { message }));
        return false;
      }
    } else {
      const err = result;
      logError("Script", `Run failed: ${err.error.message}`);
      vscode9.window.showErrorMessage(t("CanMV: {message}", { message: err.error.message }));
      return false;
    }
  }
  async stopScript() {
    const req = createRequest(Methods.stopScript, {});
    await this.requester.request(req);
  }
};

// src/service/videoService.ts
var vscode10 = __toESM(require("vscode"));

// src/service/profiler.ts
var ENABLED = process.env.CANMV_PROFILE === "1";
var FrameProfiler = class {
  constructor() {
    this.records = [];
    this.pending = null;
  }
  startFrame(frameId) {
    if (!ENABLED) return;
    this.pending = { frameId, ts: /* @__PURE__ */ new Map() };
  }
  mark(frameId, phase, ts) {
    if (!ENABLED) return;
    const record = this.pending ?? this.records.find((r) => r.frameId === frameId);
    if (record) {
      record.ts.set(phase, ts ?? performance.now());
    }
  }
  finishFrame() {
    if (!ENABLED || !this.pending) return;
    this.records.push(this.pending);
    this.pending = null;
  }
  shouldFlush() {
    return ENABLED && this.records.length >= 100;
  }
  flush() {
    if (!ENABLED || this.records.length === 0) return null;
    const batch = this.records.splice(0);
    const first = batch[0]?.frameId ?? 0;
    const last = batch[batch.length - 1]?.frameId ?? 0;
    const report = { frameRange: `${first}-${last}`, segments: {} };
    const phaseNames = /* @__PURE__ */ new Set();
    for (const r of batch) {
      for (const k of r.ts.keys()) {
        phaseNames.add(k);
      }
    }
    for (const phase of phaseNames) {
      const values = [];
      for (const r of batch) {
        const v = r.ts.get(phase);
        if (v !== void 0) values.push(v);
      }
      if (values.length >= 2) {
        report.segments[phase] = computeStats(values);
      }
    }
    return report;
  }
  /** Compute derived segment stats using absolute timestamps. */
  flushSegments() {
    if (!ENABLED || this.records.length === 0) return null;
    const batch = this.records.splice(0);
    const first = batch[0]?.frameId ?? 0;
    const last = batch[batch.length - 1]?.frameId ?? 0;
    const report = { frameRange: `${first}-${last}`, segments: {} };
    const pairs = [
      ["ts_chunk", "ts_dispatch", "VSCode Proc"],
      ["ts_dispatch", "ts_service", "Dispatch\u2192Service"],
      ["ts_service", "ts_webview_msg", "postMessage"]
    ];
    for (const [start, end, label] of pairs) {
      const deltas = [];
      for (const r of batch) {
        const a = r.ts.get(start);
        const b = r.ts.get(end);
        if (a !== void 0 && b !== void 0) deltas.push(b - a);
      }
      if (deltas.length >= 2) {
        report.segments[label] = computeStats(deltas);
      }
    }
    for (const phase of ["ts_decode_delta", "ts_draw_delta"]) {
      const label = phase === "ts_decode_delta" ? "JPEG Decode" : "Canvas Draw";
      const deltas = [];
      for (const r of batch) {
        const v = r.ts.get(phase);
        if (v !== void 0) deltas.push(v);
      }
      if (deltas.length >= 2) {
        report.segments[label] = computeStats(deltas);
      }
    }
    return report;
  }
};
function computeStats(values) {
  const sorted = [...values].sort((a, b) => a - b);
  const n = sorted.length;
  return {
    avg: sorted.reduce((s, v) => s + v, 0) / n,
    p95: sorted[Math.floor(n * 0.95)],
    max: sorted[n - 1]
  };
}

// src/service/videoService.ts
var VideoService = class {
  constructor(session, backend2, panel) {
    this.session = session;
    this.backend = backend2;
    this.panel = panel;
    this.latestFrame = null;
    this.frameSubscription = null;
    this.profiler = new FrameProfiler();
    this.frameCount = 0;
    this.previewActive = false;
    this.lastFrameAt = 0;
    this.panel.onProfile((msg) => {
      if (msg.segments) {
        for (const [label, stats] of Object.entries(msg.segments)) {
          const s = stats;
          logInfo("Profile WebView", `frames ${msg.frameRange} ${label}: avg=${s.avg.toFixed(2)}ms p95=${s.p95.toFixed(2)}ms max=${s.max.toFixed(2)}ms`);
        }
      }
    });
  }
  async startPreview(fps, resolution, options) {
    const suppressErrors = options?.suppressErrors === true;
    if (this.previewActive) {
      return true;
    }
    if (this.session.state === "streaming") {
      this.clearPreviewState();
    }
    if (this.session.state !== "connected") {
      vscode10.window.showWarningMessage(t("CanMV: Please connect to the board first."));
      return false;
    }
    if (!options?.assumeScriptRunning) {
      const runningResult = await this.session.request(createRequest(Methods.scriptRunning, {}));
      if (!isResponse(runningResult)) {
        const err = runningResult;
        logWarn("Preview", `Skipped: ${err.error.message}`);
        if (!suppressErrors) {
          vscode10.window.showWarningMessage(t("CanMV: Cannot enable preview - {message}", { message: err.error.message }));
        }
        return false;
      }
      const runningPayload = runningResult.result;
      if (!runningPayload.running) {
        logDebug("Preview", "Skipped: no script is running on board");
        return false;
      }
    }
    const req = createRequest(Methods.startPreview, { fps, resolution });
    this.frameCount = 0;
    this.frameSubscription?.dispose();
    this.frameSubscription = this.backend.onEvent((event) => {
      if (isFrameEvent(event)) {
        const params = event.params;
        this.frameCount = params.frameId;
        this.profiler.startFrame(params.frameId);
        this.profiler.mark(params.frameId, "ts_chunk", params.chunkTs);
        this.profiler.mark(params.frameId, "ts_dispatch", params.dispatchTs);
        this.profiler.mark(params.frameId, "ts_service");
        this.profiler.finishFrame();
        this.latestFrame = params.data;
        this.lastFrameAt = Date.now();
        this.panel.sendFrame(params.frameId, this.latestFrame);
        if (this.frameCount === 1 && this.firstFrameCallback) {
          this.firstFrameCallback();
        }
        if (this.frameCount === 1 || this.frameCount % 100 === 0) {
          logInfo("Preview", `Frame ${this.frameCount} delivered (${params.data.byteLength}B)`);
        }
        if (this.profiler.shouldFlush()) {
          const report = this.profiler.flushSegments();
          if (report) {
            logInfo("Profile", `frames ${report.frameRange}`);
            for (const [label, stats] of Object.entries(report.segments)) {
              logInfo("Profile", `${label}: avg=${stats.avg.toFixed(2)}ms p95=${stats.p95.toFixed(2)}ms max=${stats.max.toFixed(2)}ms`);
            }
          }
        }
      }
    });
    const result = await this.session.request(req);
    if (isResponse(result)) {
      const payload = result.result;
      if (payload.status === "error") {
        this.frameSubscription?.dispose();
        this.frameSubscription = null;
        const message = payload.message || t("Unknown preview error");
        logError("Preview", `Start failed: ${message}`);
        if (!suppressErrors) {
          vscode10.window.showErrorMessage(t("CanMV: Preview failed - {message}", { message }));
        }
        return false;
      }
      if (payload.status === "waiting") {
        this.frameSubscription?.dispose();
        this.frameSubscription = null;
        logDebug("Preview", "Waiting for framebuffer");
        return false;
      }
      logInfo("Preview", "Started");
      this.previewActive = true;
      this.session.startStreaming();
      this.panel.postMessage({ type: "started" });
      return true;
    } else {
      this.frameSubscription?.dispose();
      this.frameSubscription = null;
      const err = result;
      logError("Preview", `Start failed: ${err.error.message}`);
      if (!suppressErrors) {
        vscode10.window.showErrorMessage(t("CanMV: Preview failed - {message}", { message: err.error.message }));
      }
      return false;
    }
  }
  async stopPreview() {
    logInfo("Preview", "Stopping");
    try {
      const req = createRequest(Methods.stopPreview, {});
      await this.session.request(req);
    } finally {
      this.clearPreviewState();
    }
  }
  clearPreviewState() {
    const hadPreviewState = this.previewActive || this.frameSubscription !== null || this.latestFrame !== null;
    this.session.stopStreaming();
    this.frameSubscription?.dispose();
    this.frameSubscription = null;
    this.latestFrame = null;
    this.lastFrameAt = 0;
    this.previewActive = false;
    if (hadPreviewState && !this.panel.disposed) {
      logInfo("Preview", "Stopped");
      this.panel.postMessage({ type: "stopped" });
    }
  }
  getLatestFrame() {
    return this.latestFrame;
  }
  lastFrameAgeMs(now = Date.now()) {
    return this.lastFrameAt > 0 ? now - this.lastFrameAt : null;
  }
  hasActivePreview() {
    return this.previewActive;
  }
  onFirstFrame(callback) {
    this.firstFrameCallback = callback;
  }
};

// src/service/fileService.ts
var fs4 = __toESM(require("fs"));
var path4 = __toESM(require("path"));
var import_crypto = require("crypto");

// src/service/startupScript.ts
var STARTUP_SCRIPT_PATHS = /* @__PURE__ */ new Set([
  "/sdcard/boot.py",
  "/sdcard/main.py"
]);
var STRING_PREFIXES = ["br", "rb", "fr", "rf", "r", "u", "b", "f"];
function minifyStartupScript(remotePath, data, enabled = true) {
  if (!enabled || !isStartupScriptPath(remotePath)) return data;
  let source;
  try {
    source = new TextDecoder("utf-8", { fatal: true, ignoreBOM: true }).decode(data);
  } catch {
    return data;
  }
  return new TextEncoder().encode(minifyPythonSource(source));
}
function minifyPythonSource(source) {
  const lines = [];
  let line = "";
  let outsideStringLine = "";
  let quote;
  let tripleQuoted = false;
  let bracketDepth = 0;
  let logicalLineStart = true;
  let physicalLineStart = true;
  for (let index = 0; index < source.length; ) {
    const newlineLength = newlineLengthAt(source, index);
    if (newlineLength > 0) {
      const inTripleQuotedString = quote !== void 0 && tripleQuoted;
      appendLine(lines, line, inTripleQuotedString);
      logicalLineStart = !quote && bracketDepth === 0 && !endsWithExplicitContinuation(outsideStringLine);
      line = "";
      outsideStringLine = "";
      physicalLineStart = true;
      index += newlineLength;
      continue;
    }
    const char = source[index];
    if (!quote && physicalLineStart && logicalLineStart) {
      if (isLineWhitespace(char)) {
        line += char;
        outsideStringLine += char;
        index++;
        continue;
      }
      const standaloneBlockEnd = standaloneTripleQuotedBlockEnd(source, index);
      if (standaloneBlockEnd !== void 0) {
        line = "";
        outsideStringLine = "";
        physicalLineStart = true;
        logicalLineStart = true;
        index = standaloneBlockEnd;
        continue;
      }
    }
    if (quote) {
      if (tripleQuoted && source.startsWith(quote.repeat(3), index)) {
        line += quote.repeat(3);
        index += 3;
        quote = void 0;
        tripleQuoted = false;
      } else {
        line += char;
        if (char === "\\" && index + 1 < source.length && newlineLengthAt(source, index + 1) === 0) {
          line += source[index + 1];
          index += 2;
        } else {
          index++;
          if (!tripleQuoted && char === quote) {
            quote = void 0;
          }
        }
      }
      physicalLineStart = false;
      continue;
    }
    if (char === "#") {
      line = trimLineWhitespace(line);
      outsideStringLine = trimLineWhitespace(outsideStringLine);
      index++;
      while (index < source.length && newlineLengthAt(source, index) === 0) {
        index++;
      }
      continue;
    }
    if (char === '"' || char === "'") {
      quote = char;
      tripleQuoted = source.startsWith(char.repeat(3), index);
      if (tripleQuoted) {
        line += char.repeat(3);
        index += 3;
      } else {
        line += char;
        index++;
      }
      physicalLineStart = false;
      continue;
    }
    line += char;
    outsideStringLine += char;
    if (char === "(" || char === "[" || char === "{") {
      bracketDepth++;
    } else if ((char === ")" || char === "]" || char === "}") && bracketDepth > 0) {
      bracketDepth--;
    }
    if (!isLineWhitespace(char)) {
      physicalLineStart = false;
    }
    index++;
  }
  appendLine(lines, line, quote !== void 0 && tripleQuoted);
  return lines.join("\n");
}
function appendLine(lines, line, preserveTrailingWhitespace) {
  const minifiedLine = preserveTrailingWhitespace ? line : trimLineWhitespace(line);
  if (minifiedLine.length > 0 || preserveTrailingWhitespace) {
    lines.push(minifiedLine);
  }
}
function standaloneTripleQuotedBlockEnd(source, index) {
  const triple = tripleQuoteAt(source, index);
  if (!triple) return void 0;
  const end = tripleQuoteEnd(source, triple.contentStart, triple.delimiter);
  if (end === void 0) return void 0;
  let lineEnd = end;
  while (lineEnd < source.length && newlineLengthAt(source, lineEnd) === 0) {
    lineEnd++;
  }
  const suffix = source.slice(end, lineEnd);
  if (!/^[ \t\f]*(?:#.*)?$/.test(suffix)) return void 0;
  return lineEnd + newlineLengthAt(source, lineEnd);
}
function tripleQuoteAt(source, index) {
  for (const prefix of ["", ...STRING_PREFIXES]) {
    const quoteIndex = index + prefix.length;
    if (prefix && source.slice(index, quoteIndex).toLowerCase() !== prefix) continue;
    const quote = source[quoteIndex];
    if ((quote === '"' || quote === "'") && source.startsWith(quote.repeat(3), quoteIndex)) {
      return { delimiter: quote.repeat(3), contentStart: quoteIndex + 3 };
    }
  }
  return void 0;
}
function tripleQuoteEnd(source, index, delimiter2) {
  for (let cursor = index; cursor < source.length; ) {
    if (source[cursor] === "\\" && cursor + 1 < source.length) {
      cursor += newlineLengthAt(source, cursor + 1) === 0 ? 2 : 1;
      continue;
    }
    if (source.startsWith(delimiter2, cursor)) {
      return cursor + delimiter2.length;
    }
    cursor++;
  }
  return void 0;
}
function endsWithExplicitContinuation(line) {
  const trimmed = trimLineWhitespace(line);
  let slashCount = 0;
  for (let index = trimmed.length - 1; index >= 0 && trimmed[index] === "\\"; index--) {
    slashCount++;
  }
  return slashCount % 2 === 1;
}
function trimLineWhitespace(value) {
  return value.replace(/[ \t\f]+$/g, "");
}
function isLineWhitespace(char) {
  return char === " " || char === "	" || char === "\f";
}
function newlineLengthAt(source, index) {
  if (source[index] === "\r") return source[index + 1] === "\n" ? 2 : 1;
  return source[index] === "\n" ? 1 : 0;
}
function isStartupScriptPath(remotePath) {
  const normalized = "/" + remotePath.replace(/\\/g, "/").replace(/^\/+/, "").replace(/\/+/g, "/").replace(/\/+$/g, "");
  return STARTUP_SCRIPT_PATHS.has(normalized);
}

// src/service/fileService.ts
var REMOTE_FILE_WRITE_CHUNK_SIZE = 8 * 1024;
var REMOTE_FILE_READ_CHUNK_SIZE = 32 * 1024;
var REMOTE_FILE_CHUNK_TIMEOUT_MS = 15e3;
var REMOTE_FILE_VERIFY_TIMEOUT_MS = 6 * 6e4;
var REMOTE_DIRECTORY_LIST_TIMEOUT_MS = 3e4;
var MAX_DIRECTORY_LIST_PAGES = 1e5;
function mutationSucceeded(result) {
  return !!result.success;
}
function joinRemotePath(parent, name) {
  return parent === "/" ? "/" + name : parent.replace(/\/+$/g, "") + "/" + name;
}
var FileService = class {
  constructor(requester, shouldMinifyStartupScripts = () => true) {
    this.requester = requester;
    this.shouldMinifyStartupScripts = shouldMinifyStartupScripts;
    this.readCache = /* @__PURE__ */ new Map();
    this.fileOperationTail = Promise.resolve();
  }
  async listDir(path15) {
    return this.runFileOperation(async () => {
      const entries = [];
      let offset = 0;
      for (let pageCount = 0; pageCount < MAX_DIRECTORY_LIST_PAGES; pageCount++) {
        const page = await this.requestListDirPage(path15, offset);
        entries.push(...page.entries);
        if (page.nextOffset === void 0) {
          return entries;
        }
        offset = page.nextOffset;
      }
      throw new Error(`Directory listing exceeded ${MAX_DIRECTORY_LIST_PAGES} pages: ${path15}`);
    });
  }
  async listDirPage(path15, offset = 0) {
    return this.runFileOperation(() => this.requestListDirPage(path15, offset));
  }
  async requestListDirPage(path15, offset) {
    const req = createRequest(Methods.ioListDir, { path: path15, offset });
    const result = await this.requester.request(req, { timeoutMs: REMOTE_DIRECTORY_LIST_TIMEOUT_MS });
    if (!isResponse(result)) {
      const message = result.error.message;
      logWarn("Files", `List failed: ${path15}: ${message}`);
      throw new Error(message);
    }
    const page = result.result;
    if (!Array.isArray(page.entries)) {
      throw new Error(`Invalid directory listing response for ${path15}`);
    }
    if (page.nextOffset !== void 0 && (!Number.isSafeInteger(page.nextOffset) || page.nextOffset <= offset || page.nextOffset > 4294967295)) {
      throw new Error(`Invalid directory listing continuation for ${path15}`);
    }
    return { entries: page.entries, nextOffset: page.nextOffset };
  }
  async statFile(path15) {
    return this.runFileOperation(() => this.requestFileStat(path15));
  }
  async requestFileStat(path15) {
    const req = createRequest(Methods.ioQueryFileStat, { path: path15 });
    const result = await this.requester.request(req);
    if (isResponse(result)) {
      return result.result;
    }
    const message = result.error.message;
    logWarn("Files", `Stat failed: ${path15}: ${message}`);
    throw new Error(message);
  }
  async readFile(path15, options) {
    return this.runFileOperation(async () => {
      const shouldLogSuccess = options?.logSuccess ?? true;
      const cacheKey = normalizeRemotePath(path15);
      const startedAt = Date.now();
      const stat = await this.requestFileStat(cacheKey);
      if (!stat.exists) {
        logWarn("Files", `Read failed: ${cacheKey}: file not found`);
        throw new Error(t("Remote file not found: {path}", { path: cacheKey }));
      }
      if (stat.type === "directory") {
        logWarn("Files", `Read failed: ${cacheKey}: path is a folder`);
        throw new Error(t("Remote path is a folder: {path}", { path: cacheKey }));
      }
      const cached = this.readCache.get(cacheKey);
      if (cached && cacheMatchesStat(cached, stat)) {
        if (shouldLogSuccess) {
          logDebug("Files", `Read cache hit: ${cacheKey} (${formatFileSize(cached.size)})`);
        }
        options?.onChunk?.(cached.size);
        return new Uint8Array(cached.data);
      }
      if (cached && shouldLogSuccess) {
        logDebug("Files", `Read cache stale: ${cacheKey}`);
      }
      const data = stat.size > REMOTE_FILE_READ_CHUNK_SIZE ? await this.readFileInChunks(cacheKey, stat.size, options?.onChunk) : await this.readFilePayload(cacheKey);
      if (stat.size <= REMOTE_FILE_READ_CHUNK_SIZE) {
        options?.onChunk?.(data.byteLength);
      }
      if (data.byteLength !== stat.size) {
        this.invalidateCache(cacheKey);
        logError("Files", `Read incomplete: ${cacheKey}: expected ${formatFileSize(stat.size)}, got ${formatFileSize(data.byteLength)}`);
        throw new Error(t("Read incomplete: expected {expected} bytes, got {actual}", { expected: stat.size, actual: data.byteLength }));
      }
      this.readCache.set(cacheKey, {
        data: new Uint8Array(data),
        size: stat.size,
        mtime: stat.mtime
      });
      if (shouldLogSuccess) {
        logInfo("Files", `Read ${cacheKey} (${formatFileSize(data.byteLength)}, ${Date.now() - startedAt}ms)`);
      }
      return data;
    });
  }
  async readFileInChunks(remotePath, fileSize, onChunk) {
    const data = new Uint8Array(fileSize);
    let offset = 0;
    while (offset < fileSize) {
      const size = Math.min(REMOTE_FILE_READ_CHUNK_SIZE, fileSize - offset);
      const chunk = await this.readFilePayload(remotePath, { offset, size });
      if (chunk.byteLength === 0 || chunk.byteLength > size) {
        this.invalidateCache(remotePath);
        logError("Files", `Read incomplete: ${remotePath} at offset ${offset}: expected at most ${formatFileSize(size)}, got ${formatFileSize(chunk.byteLength)}`);
        throw new Error(t("Read incomplete: expected {expected} bytes, got {actual}", { expected: fileSize, actual: offset + chunk.byteLength }));
      }
      data.set(chunk, offset);
      offset += chunk.byteLength;
      onChunk?.(chunk.byteLength);
    }
    return data;
  }
  async readFilePayload(remotePath, range) {
    const req = createRequest(Methods.ioReadFile, range ? { path: remotePath, ...range } : { path: remotePath });
    const result = await this.requester.request(req, { timeoutMs: REMOTE_FILE_CHUNK_TIMEOUT_MS });
    if (isResponse(result)) {
      return decodeFilePayload(result.result);
    }
    const message = result.error.message;
    logWarn("Files", `Read failed: ${remotePath}: ${message}`);
    throw new Error(message);
  }
  async writeFile(path15, data, options) {
    return this.runFileOperation(async () => {
      const shouldLogSuccess = options?.logSuccess ?? true;
      const startedAt = Date.now();
      const writeData = minifyStartupScript(path15, data, this.shouldMinifyStartupScripts());
      const success = await this.writeFileInChunks(path15, writeData, options?.onChunk, options?.onPhase);
      if (success) {
        await this.updateCachedWrite(path15, writeData);
        if (shouldLogSuccess) {
          logInfo("Files", `Wrote ${path15} (${formatFileSize(writeData.byteLength)}, ${Date.now() - startedAt}ms)`);
        }
      } else {
        this.invalidateCache(path15);
        logWarn("Files", `Write rejected: ${path15} (${formatFileSize(writeData.byteLength)})`);
      }
      return success;
    });
  }
  async writeFileInChunks(remotePath, data, onChunk, onPhase) {
    const sha256Base64 = (0, import_crypto.createHash)("sha256").update(data).digest("base64");
    let active = false;
    try {
      if (!await this.beginFileWrite(remotePath, data.byteLength, sha256Base64)) return false;
      active = true;
      onPhase?.("transferring");
      for (let offset = 0; offset < data.byteLength; offset += REMOTE_FILE_WRITE_CHUNK_SIZE) {
        const chunk = data.subarray(offset, Math.min(offset + REMOTE_FILE_WRITE_CHUNK_SIZE, data.byteLength));
        if (!await this.writeFileChunk(remotePath, chunk)) {
          active = false;
          return false;
        }
        onChunk?.(chunk.byteLength);
      }
      onPhase?.("verifying");
      const success = await this.finishFileWrite(remotePath);
      active = false;
      return success;
    } finally {
      if (active) await this.abortFileWrite(remotePath);
    }
  }
  async writeLocalFileInChunks(localPath, remotePath, size, onChunk, onPhase) {
    onPhase?.("hashing");
    const sha256Base64 = await this.hashLocalFile(localPath);
    let active = false;
    try {
      if (!await this.beginFileWrite(remotePath, size, sha256Base64)) return false;
      active = true;
      onPhase?.("transferring");
      for await (const chunk of fs4.createReadStream(localPath, { highWaterMark: REMOTE_FILE_WRITE_CHUNK_SIZE })) {
        const data = Buffer.isBuffer(chunk) ? chunk : Buffer.from(chunk);
        if (!await this.writeFileChunk(remotePath, data)) {
          active = false;
          return false;
        }
        onChunk?.(data.byteLength);
      }
      onPhase?.("verifying");
      const success = await this.finishFileWrite(remotePath);
      active = false;
      return success;
    } finally {
      if (active) await this.abortFileWrite(remotePath);
    }
  }
  async hashLocalFile(localPath) {
    const hash = (0, import_crypto.createHash)("sha256");
    for await (const chunk of fs4.createReadStream(localPath, { highWaterMark: REMOTE_FILE_READ_CHUNK_SIZE })) {
      hash.update(chunk);
    }
    return hash.digest("base64");
  }
  async beginFileWrite(remotePath, size, sha256Base64) {
    return this.writeFileRequest(
      createRequest(Methods.ioBeginWriteFile, { path: remotePath, size, sha256Base64 }),
      REMOTE_FILE_CHUNK_TIMEOUT_MS,
      remotePath
    );
  }
  async writeFileChunk(remotePath, data) {
    return this.writeFileRequest(
      createRequest(Methods.ioWriteFileChunk, { dataBase64: Buffer.from(data).toString("base64") }),
      REMOTE_FILE_CHUNK_TIMEOUT_MS,
      remotePath
    );
  }
  async finishFileWrite(remotePath) {
    return this.writeFileRequest(
      createRequest(Methods.ioFinishWriteFile, {}),
      REMOTE_FILE_VERIFY_TIMEOUT_MS,
      remotePath
    );
  }
  async abortFileWrite(remotePath) {
    try {
      const req = createRequest(Methods.ioAbortWriteFile, {});
      const result = await this.requester.request(req, { timeoutMs: REMOTE_FILE_CHUNK_TIMEOUT_MS });
      if (!isResponse(result) || !mutationSucceeded(result.result)) {
        logWarn("Files", `Write cleanup failed: ${remotePath}`);
      }
    } catch (error) {
      logWarn("Files", `Write cleanup failed: ${remotePath}: ${error instanceof Error ? error.message : String(error)}`);
    }
  }
  async writeFileRequest(req, timeoutMs, remotePath) {
    const result = await this.requester.request(req, { timeoutMs });
    if (isResponse(result)) {
      return mutationSucceeded(result.result);
    }
    const message = result.error.message;
    logWarn("Files", `Write failed: ${remotePath}: ${message}`);
    throw new Error(message);
  }
  async fileExec(path15) {
    return this.runFileOperation(async () => {
      const startedAt = Date.now();
      const req = createRequest(Methods.ioFileExec, { path: path15 });
      const result = await this.requester.request(req);
      if (isResponse(result)) {
        const payload = result.result;
        logInfo("Files", `Executed ${path15}: ${payload.status} (${Date.now() - startedAt}ms)`);
        return payload;
      }
      const message = result.error.message;
      logWarn("Files", `Execute failed: ${path15}: ${message}`);
      throw new Error(message);
    });
  }
  async deleteFile(path15) {
    return this.runFileOperation(async () => {
      const req = createRequest(Methods.ioDeleteFile, { path: path15 });
      const result = await this.requester.request(req);
      if (isResponse(result)) {
        const success = mutationSucceeded(result.result);
        if (success) {
          this.invalidateCache(path15);
          logInfo("Files", `Deleted file: ${path15}`);
        } else {
          logWarn("Files", `Delete file rejected: ${path15}`);
        }
        return success;
      }
      const message = result.error.message;
      logWarn("Files", `Delete file failed: ${path15}: ${message}`);
      throw new Error(message);
    });
  }
  async renameFile(oldPath, newPath) {
    return this.runFileOperation(async () => {
      const req = createRequest(Methods.ioRenameFile, { oldPath, newPath });
      const result = await this.requester.request(req);
      if (isResponse(result)) {
        const success = mutationSucceeded(result.result);
        if (success) {
          this.invalidateCache(oldPath, true);
          this.invalidateCache(newPath, true);
          logInfo("Files", `Renamed: ${oldPath} -> ${newPath}`);
        } else {
          logWarn("Files", `Rename rejected: ${oldPath} -> ${newPath}`);
        }
        return success;
      }
      const message = result.error.message;
      logWarn("Files", `Rename failed: ${oldPath} -> ${newPath}: ${message}`);
      throw new Error(message);
    });
  }
  async mkdir(path15, options) {
    return this.runFileOperation(async () => {
      const shouldLogSuccess = options?.logSuccess ?? true;
      const shouldLogRejected = options?.logRejected ?? true;
      const req = createRequest(Methods.ioMkdir, { path: path15 });
      const result = await this.requester.request(req);
      if (isResponse(result)) {
        const success = mutationSucceeded(result.result);
        if (success) {
          this.invalidateCache(path15, true);
          if (shouldLogSuccess) {
            logInfo("Files", `Created folder: ${path15}`);
          }
        } else if (shouldLogRejected) {
          logWarn("Files", `Create folder rejected: ${path15}`);
        }
        return success;
      }
      const message = result.error.message;
      logWarn("Files", `Create folder failed: ${path15}: ${message}`);
      throw new Error(message);
    });
  }
  clearCache() {
    this.readCache.clear();
    logDebug("Files", "Cleared file read cache");
  }
  async rmdir(path15, recursive = false) {
    return this.runFileOperation(async () => {
      const req = createRequest(Methods.ioRmdir, { path: path15, recursive });
      const result = await this.requester.request(req);
      if (isResponse(result)) {
        const payload = result.result;
        const success = mutationSucceeded(payload);
        if (success) {
          this.invalidateCache(path15, true);
          logInfo("Files", `Deleted folder: ${path15}`);
        } else {
          const message2 = payload.message || `Device error ${payload.errorCode ?? "unknown"}`;
          logWarn("Files", `Delete folder rejected: ${path15}: ${message2}`);
          throw new Error(message2);
        }
        return success;
      }
      const message = result.error.message;
      logWarn("Files", `Delete folder failed: ${path15}: ${message}`);
      throw new Error(message);
    });
  }
  measureUpload(localPath, remotePath) {
    const totals = { bytes: 0, files: 0 };
    this.measureUploadPath(localPath, remotePath, totals);
    return totals;
  }
  measureUploadPath(localPath, remotePath, totals) {
    const stat = fs4.statSync(localPath);
    if (stat.isFile()) {
      totals.files++;
      totals.bytes += this.uploadFileSize(localPath, remotePath, stat.size);
      return;
    }
    if (!stat.isDirectory()) return;
    for (const entry of fs4.readdirSync(localPath, { withFileTypes: true })) {
      const localChild = path4.join(localPath, entry.name);
      const remoteChild = joinRemotePath(remotePath, entry.name);
      if (entry.isDirectory() || entry.isFile()) {
        this.measureUploadPath(localChild, remoteChild, totals);
      }
    }
  }
  uploadFileSize(localPath, remotePath, fallbackSize) {
    if (!this.shouldMinifyStartupScripts() || !isStartupScriptPath(remotePath)) return fallbackSize;
    return minifyStartupScript(remotePath, fs4.readFileSync(localPath), true).byteLength;
  }
  async upload(localPath, remotePath, report) {
    const stat = fs4.statSync(localPath);
    const startedAt = Date.now();
    const stats = { files: 0, folders: 0, bytes: 0 };
    const totals = this.measureUpload(localPath, remotePath);
    const context = {
      ...totals,
      bytesTransferred: 0,
      filesTransferred: 0,
      report
    };
    this.reportTransfer(context, "transferring", remotePath);
    if (stat.isDirectory()) {
      logInfo("Files", `Upload folder started: ${localPath} -> ${remotePath}`);
      await this.uploadDirectory(localPath, remotePath, stats, context);
      logInfo("Files", `Upload folder finished: ${localPath} -> ${remotePath} (${describeTransfer(stats)}, ${Date.now() - startedAt}ms)`);
      return;
    }
    if (!stat.isFile()) {
      logWarn("Files", `Upload rejected: ${localPath}: not a file or folder`);
      throw new Error(t("Only files and folders can be uploaded"));
    }
    const ok = await this.uploadLocalFile(localPath, remotePath, stat.size, context);
    if (!ok) throw new Error(t("Failed to upload {name}", { name: path4.basename(localPath) }));
    context.filesTransferred++;
    stats.files = 1;
    stats.bytes = totals.bytes;
    this.reportTransfer(context, "transferring", remotePath);
    logInfo("Files", `Upload file finished: ${localPath} -> ${remotePath} (${formatFileSize(stat.size)}, ${Date.now() - startedAt}ms)`);
  }
  async uploadDirectory(localDir, remoteDir, stats, context) {
    const made = await this.mkdir(remoteDir, { logSuccess: false, logRejected: false });
    if (!made) {
      try {
        const entries2 = await this.listDir(remoteDir);
        if (!Array.isArray(entries2)) throw new Error(t("not a directory"));
      } catch {
        throw new Error(t("Failed to create remote folder {path}", { path: remoteDir }));
      }
    }
    stats.folders++;
    const entries = fs4.readdirSync(localDir, { withFileTypes: true });
    for (const entry of entries) {
      const localChild = path4.join(localDir, entry.name);
      const remoteChild = joinRemotePath(remoteDir, entry.name);
      if (entry.isDirectory()) {
        await this.uploadDirectory(localChild, remoteChild, stats, context);
      } else if (entry.isFile()) {
        const localStat = fs4.statSync(localChild);
        const ok = await this.uploadLocalFile(localChild, remoteChild, localStat.size, context);
        if (!ok) throw new Error(t("Failed to upload {path}", { path: localChild }));
        stats.files++;
        const transferredSize = this.uploadFileSize(localChild, remoteChild, localStat.size);
        stats.bytes += transferredSize;
        context.filesTransferred++;
        this.reportTransfer(context, "transferring", remoteChild);
      }
    }
  }
  async uploadLocalFile(localPath, remotePath, size, context) {
    const onChunk = (bytes) => {
      context.bytesTransferred += bytes;
      this.reportTransfer(context, "transferring", remotePath);
    };
    const onPhase = (phase) => this.reportTransfer(context, phase, remotePath);
    if (this.shouldMinifyStartupScripts() && isStartupScriptPath(remotePath)) {
      const data = fs4.readFileSync(localPath);
      return this.writeFile(remotePath, data, { logSuccess: false, onChunk, onPhase });
    }
    const success = await this.runFileOperation(
      () => this.writeLocalFileInChunks(localPath, remotePath, size, onChunk, onPhase)
    );
    this.invalidateCache(remotePath);
    return success;
  }
  async runFileOperation(operation) {
    const previous = this.fileOperationTail;
    let release;
    const current = new Promise((resolve3) => {
      release = resolve3;
    });
    this.fileOperationTail = previous.then(() => current);
    await previous;
    try {
      return await operation();
    } finally {
      release();
    }
  }
  async download(remotePath, localPath, report) {
    const startedAt = Date.now();
    report?.({
      phase: "scanning",
      path: remotePath,
      bytesTransferred: 0,
      totalBytes: 0,
      filesTransferred: 0,
      totalFiles: 0
    });
    const plan = await this.buildDownloadPlan(remotePath, localPath);
    const context = {
      bytes: plan.bytes,
      files: plan.fileEntries.length,
      bytesTransferred: 0,
      filesTransferred: 0,
      report
    };
    this.reportTransfer(context, "transferring", remotePath);
    if (plan.directories.length > 0) {
      logInfo("Files", `Download folder started: ${remotePath} -> ${localPath}`);
      for (const directory of plan.directories) {
        if (fs4.existsSync(directory) && !fs4.statSync(directory).isDirectory()) {
          logWarn("Files", `Download failed: local path is not a folder: ${directory}`);
          throw new Error(t("Local path exists and is not a folder: {path}", { path: directory }));
        }
        fs4.mkdirSync(directory, { recursive: true });
      }
      for (const file of plan.fileEntries) {
        await this.downloadPlannedFile(file, context);
      }
      const stats = {
        files: context.filesTransferred,
        folders: plan.directories.length,
        bytes: context.bytesTransferred
      };
      logInfo("Files", `Download folder finished: ${remotePath} -> ${localPath} (${describeTransfer(stats)}, ${Date.now() - startedAt}ms)`);
      return;
    }
    await this.downloadPlannedFile(plan.fileEntries[0], context);
    logInfo("Files", `Download file finished: ${remotePath} -> ${localPath} (${formatFileSize(context.bytesTransferred)}, ${Date.now() - startedAt}ms)`);
  }
  async addDownloadDirectory(remoteDir, localDir, plan) {
    plan.directories.push(localDir);
    const entries = await this.listDir(remoteDir);
    for (const entry of entries) {
      if (entry.name === "." || entry.name === "..") continue;
      const remoteChild = joinRemotePath(remoteDir, entry.name);
      const localChild = path4.join(localDir, entry.name);
      if (entry.type === "directory") {
        await this.addDownloadDirectory(remoteChild, localChild, plan);
      } else {
        plan.fileEntries.push({ remotePath: remoteChild, localPath: localChild, size: entry.size });
        plan.bytes += entry.size;
      }
    }
  }
  async buildDownloadPlan(remotePath, localPath) {
    const plan = { directories: [], fileEntries: [], bytes: 0 };
    const stat = await this.statFile(remotePath);
    if (!stat.exists) {
      logWarn("Files", `Download failed: ${remotePath}: remote path not found`);
      throw new Error(t("Remote path not found: {path}", { path: remotePath }));
    }
    if (stat.type === "directory") {
      await this.addDownloadDirectory(remotePath, localPath, plan);
    } else {
      plan.fileEntries.push({ remotePath, localPath, size: stat.size });
      plan.bytes = stat.size;
    }
    return plan;
  }
  async downloadPlannedFile(file, context) {
    let localPath = file.localPath;
    if (fs4.existsSync(localPath) && fs4.statSync(localPath).isDirectory()) {
      localPath = path4.join(localPath, path4.basename(file.remotePath));
    }
    fs4.mkdirSync(path4.dirname(localPath), { recursive: true });
    const data = await this.readFile(file.remotePath, {
      logSuccess: false,
      onChunk: (bytes) => {
        context.bytesTransferred += bytes;
        this.reportTransfer(context, "transferring", file.remotePath);
      }
    });
    fs4.writeFileSync(localPath, data);
    context.filesTransferred++;
    this.reportTransfer(context, "transferring", file.remotePath);
  }
  reportTransfer(context, phase, path15) {
    context.report?.({
      phase,
      path: path15,
      bytesTransferred: context.bytesTransferred,
      totalBytes: context.bytes,
      filesTransferred: context.filesTransferred,
      totalFiles: context.files
    });
  }
  async updateCachedWrite(path15, data) {
    const cacheKey = normalizeRemotePath(path15);
    try {
      const stat = await this.requestFileStat(cacheKey);
      this.readCache.set(cacheKey, {
        data: new Uint8Array(data),
        size: stat.size,
        mtime: stat.mtime
      });
    } catch {
      this.invalidateCache(cacheKey);
    }
  }
  invalidateCache(path15, recursive = false) {
    const cacheKey = normalizeRemotePath(path15);
    if (!recursive) {
      this.readCache.delete(cacheKey);
      return;
    }
    const prefix = cacheKey === "/" ? "/" : cacheKey + "/";
    for (const key of this.readCache.keys()) {
      if (key === cacheKey || key.startsWith(prefix)) {
        this.readCache.delete(key);
      }
    }
  }
};
function describeTransfer(stats) {
  return `${stats.files} file${stats.files === 1 ? "" : "s"}, ${stats.folders} folder${stats.folders === 1 ? "" : "s"}, ${formatFileSize(stats.bytes)}`;
}
function formatFileSize(size) {
  if (!Number.isFinite(size) || size < 0) return "0 B";
  if (size < 1024) return `${size} B`;
  const units = ["KiB", "MiB", "GiB", "TiB"];
  let value = size / 1024;
  let unit = units[0];
  for (let i = 1; i < units.length && value >= 1024; i++) {
    value /= 1024;
    unit = units[i];
  }
  const digits = value < 10 ? 1 : 0;
  return `${value.toFixed(digits)} ${unit}`;
}
function normalizeRemotePath(path15) {
  if (!path15 || path15 === "/") return "/";
  return path15.replace(/\/+$/g, "") || "/";
}
function cacheMatchesStat(cached, stat) {
  if (!stat.exists || stat.type === "directory" || cached.size !== stat.size) {
    return false;
  }
  if (typeof stat.mtime === "number" && Number.isFinite(stat.mtime) && stat.mtime > 0) {
    return cached.mtime === stat.mtime;
  }
  return true;
}
function decodeFilePayload(payload) {
  if (typeof payload.dataBase64 === "string") {
    return new Uint8Array(Buffer.from(payload.dataBase64, "base64"));
  }
  return new Uint8Array(payload.data || []);
}

// src/service/stubsService.ts
var vscode11 = __toESM(require("vscode"));
var fs6 = __toESM(require("fs"));
var path6 = __toESM(require("path"));
var os2 = __toESM(require("os"));
var crypto = __toESM(require("crypto"));
var import_child_process = require("child_process");

// src/service/resourceRouteService.ts
var https = __toESM(require("https"));
var http = __toESM(require("http"));
var fs5 = __toESM(require("fs"));
var path5 = __toESM(require("path"));
var os = __toESM(require("os"));
var CANMV_RESOURCE_BASE_URL = "https://download.kendryte.com/developer/tools/canmv_vscode_extension";
function normalizeFirmwareRevision(revision) {
  const trimmed = (revision || "").trim();
  return /^[0-9a-fA-F]{40}$/.test(trimmed) ? trimmed.toLowerCase() : "";
}
function normalizeExamplesId(examplesId) {
  const trimmed = (examplesId || "").trim();
  return /^[0-9a-fA-F]{64}$/.test(trimmed) ? trimmed.toLowerCase() : "";
}
var CanmvResourceRouteService = class {
  constructor() {
    this.assetsBaseUrl = CANMV_RESOURCE_BASE_URL;
    this.cacheRoot = path5.join(os.homedir(), ".kendryte", "k230_canmv_resources");
    this.manifestCache = /* @__PURE__ */ new Map();
    this.latestRevisionCache = "";
  }
  async resolve(boardRevision) {
    const requestedRevision = normalizeFirmwareRevision(boardRevision);
    if (requestedRevision) {
      const exactManifest = await this.fetchFirmwareManifest(requestedRevision);
      if (exactManifest) {
        return this.routeFromManifest(exactManifest, requestedRevision, true);
      }
      const latest2 = await this.fetchLatestRevision();
      if (latest2) {
        logWarn("Resources", `Firmware manifest not found for ${requestedRevision}; using latest ${latest2}`);
        const latestManifest2 = await this.fetchFirmwareManifest(latest2);
        if (latestManifest2) {
          return this.routeFromManifest(latestManifest2, requestedRevision, false);
        }
        return this.directRoute(latest2, requestedRevision, false);
      }
      return null;
    }
    const latest = await this.fetchLatestRevision();
    if (!latest) return null;
    const latestManifest = await this.fetchFirmwareManifest(latest);
    return latestManifest ? this.routeFromManifest(latestManifest, "", true) : this.directRoute(latest, "", true);
  }
  async resolveRevision(revision) {
    const normalized = normalizeFirmwareRevision(revision);
    if (!normalized) return null;
    const manifest = await this.fetchFirmwareManifest(normalized);
    return manifest ? this.routeFromManifest(manifest, normalized, true) : this.directRoute(normalized, normalized, true);
  }
  async fetchBuffer(url) {
    return new Promise((resolve3, reject) => {
      const get3 = url.startsWith("https") ? https.get : http.get;
      const req = get3(url, { timeout: 3e4 }, (res) => {
        if (res.statusCode === 301 || res.statusCode === 302 || res.statusCode === 307 || res.statusCode === 308) {
          const redirectUrl = res.headers.location;
          if (redirectUrl) {
            this.fetchBuffer(redirectUrl).then(resolve3).catch(reject);
            return;
          }
        }
        if (res.statusCode !== 200) {
          res.resume();
          reject(new Error(`HTTP ${res.statusCode}`));
          return;
        }
        const chunks = [];
        res.on("data", (chunk) => chunks.push(chunk));
        res.on("end", () => resolve3(Buffer.concat(chunks)));
        res.on("error", reject);
      });
      req.on("error", reject);
      req.on("timeout", () => {
        req.destroy();
        reject(new Error("timeout"));
      });
    });
  }
  async fetchLatestRevision() {
    const latestUrl = `${this.assetsBaseUrl}/firmware/latest`;
    logInfo("Resources", `Fetching latest firmware revision: ${latestUrl}`);
    try {
      const data = await this.fetchBuffer(latestUrl);
      const text = data.toString("utf-8").trim();
      const exact = normalizeFirmwareRevision(text);
      if (exact) {
        logInfo("Resources", `Latest firmware revision found: ${exact}`);
        this.latestRevisionCache = exact;
        this.writeCachedLatestRevision(exact);
        return exact;
      }
      const match = text.match(/\b([0-9a-fA-F]{40})\b/);
      const extracted = match ? normalizeFirmwareRevision(match[1]) : "";
      if (extracted) {
        logInfo("Resources", `Extracted latest firmware revision: ${extracted}`);
        this.latestRevisionCache = extracted;
        this.writeCachedLatestRevision(extracted);
        return extracted;
      }
      logWarn("Resources", `Unexpected firmware/latest response: "${text.substring(0, 80)}"`);
      return "";
    } catch (err) {
      logWarn("Resources", `Failed to fetch firmware/latest: ${err}`);
      const cached = this.latestRevisionCache || this.readCachedLatestRevision();
      if (cached) {
        logInfo("Resources", `Using cached latest firmware revision: ${cached}`);
        return cached;
      }
      return "";
    }
  }
  async fetchFirmwareManifest(revision) {
    const normalized = normalizeFirmwareRevision(revision);
    if (!normalized) return null;
    const cached = this.manifestCache.get(normalized) || this.readCachedFirmwareManifest(normalized);
    if (cached) {
      logInfo("Resources", `Using cached firmware manifest: ${this.manifestUrl(normalized)}`);
      this.manifestCache.set(normalized, cached);
      return cached;
    }
    const manifestUrl = this.manifestUrl(normalized);
    logInfo("Resources", `Fetching firmware manifest: ${manifestUrl}`);
    try {
      const data = await this.fetchBuffer(manifestUrl);
      const manifest = JSON.parse(data.toString("utf-8"));
      const manifestRevision = normalizeFirmwareRevision(manifest.firmware_commit || normalized);
      if (!manifestRevision) {
        logWarn("Resources", `Firmware manifest has invalid revision: ${manifestUrl}`);
        return null;
      }
      manifest.firmware_commit = manifestRevision;
      this.manifestCache.set(manifestRevision, manifest);
      this.writeCachedFirmwareManifest(manifestRevision, manifest);
      return manifest;
    } catch (err) {
      logWarn("Resources", `Failed to fetch firmware manifest ${normalized}: ${err}`);
      return null;
    }
  }
  routeFromManifest(manifest, requestedRevision, exact) {
    const revision = normalizeFirmwareRevision(manifest.firmware_commit || requestedRevision);
    const examplesId = normalizeExamplesId(manifest.examples?.id || "");
    return {
      requestedRevision,
      revision,
      exact,
      manifest,
      stubsUrl: this.resolveManifestUrl(revision, manifest.stubs?.url) || this.stubsUrl(revision),
      examplesId,
      examplesUrl: examplesId ? this.resolveManifestUrl(revision, manifest.examples?.url) : ""
    };
  }
  directRoute(revision, requestedRevision, exact) {
    return {
      requestedRevision,
      revision,
      exact,
      manifest: null,
      stubsUrl: this.stubsUrl(revision),
      examplesId: "",
      examplesUrl: ""
    };
  }
  resolveManifestUrl(revision, maybeUrl) {
    if (!maybeUrl) return "";
    try {
      return new URL(maybeUrl, this.manifestUrl(revision)).toString();
    } catch {
      return "";
    }
  }
  manifestUrl(revision) {
    return `${this.assetsBaseUrl}/firmware/${revision}/manifest.json`;
  }
  stubsUrl(revision) {
    return `${this.assetsBaseUrl}/stubs/${revision}.zip`;
  }
  cachedLatestPath() {
    return path5.join(this.cacheRoot, "firmware", "latest");
  }
  cachedManifestPath(revision) {
    return path5.join(this.cacheRoot, "firmware", revision, "manifest.json");
  }
  readCachedLatestRevision() {
    try {
      const revision = normalizeFirmwareRevision(fs5.readFileSync(this.cachedLatestPath(), "utf-8").trim());
      if (revision) this.latestRevisionCache = revision;
      return revision;
    } catch {
      return "";
    }
  }
  writeCachedLatestRevision(revision) {
    const normalized = normalizeFirmwareRevision(revision);
    if (!normalized) return;
    try {
      const latestPath = this.cachedLatestPath();
      fs5.mkdirSync(path5.dirname(latestPath), { recursive: true });
      fs5.writeFileSync(latestPath, `${normalized}
`);
    } catch {
    }
  }
  readCachedFirmwareManifest(revision) {
    const normalized = normalizeFirmwareRevision(revision);
    if (!normalized) return null;
    try {
      const manifest = JSON.parse(fs5.readFileSync(this.cachedManifestPath(normalized), "utf-8"));
      const manifestRevision = normalizeFirmwareRevision(manifest.firmware_commit || normalized);
      if (!manifestRevision) return null;
      manifest.firmware_commit = manifestRevision;
      return manifest;
    } catch {
      return null;
    }
  }
  writeCachedFirmwareManifest(revision, manifest) {
    const normalized = normalizeFirmwareRevision(revision);
    if (!normalized) return;
    try {
      const manifestPath = this.cachedManifestPath(normalized);
      fs5.mkdirSync(path5.dirname(manifestPath), { recursive: true });
      fs5.writeFileSync(manifestPath, `${JSON.stringify(manifest, null, 2)}
`);
    } catch {
    }
  }
};

// src/service/stubsService.ts
var StubsService = class _StubsService {
  constructor(context, routeService = new CanmvResourceRouteService()) {
    this.context = context;
    this.routeService = routeService;
    this.boardRevisionRequested = "";
    this.pylanceWarningShown = false;
    this.reloadPromptSignature = "";
    this.baseDir = path6.join(os2.homedir(), ".kendryte", "k230_canmv_stubs");
    this.pylanceOverlayBaseDir = path6.join(os2.homedir(), ".kendryte", "k230_canmv_pylance");
  }
  static {
    this.lastRevisionKey = "canmv.stubs.lastRevision";
  }
  static {
    this.userStubPathKey = "canmv.stubs.userStubPath";
  }
  static {
    this.reloadPromptSignatureKey = "canmv.stubs.reloadPromptSignature";
  }
  static {
    this.overlayManifestFile = ".canmv-pylance-overlay.json";
  }
  static {
    this.pylanceExtensionId = "ms-python.vscode-pylance";
  }
  async ensureDefaultStubs() {
    const lastRevision = this.context?.globalState.get(_StubsService.lastRevisionKey) || "";
    if (this.isCacheUsable(lastRevision)) {
      logInfo("Stubs", `Using last configured local stubs: ${lastRevision}`);
      return this.configureRevision(lastRevision, "default");
    }
    const localRevision = this.findLatestLocalRevision();
    if (localRevision) {
      logInfo("Stubs", `Using latest local cached stubs: ${localRevision}`);
      return this.configureRevision(localRevision, "default");
    }
    if (!this.canAutoDownload()) {
      return null;
    }
    const route = await this.routeService.resolve("");
    if (!route) {
      logWarn("Stubs", "Failed to resolve latest CanMV resources from CDN");
      return null;
    }
    logInfo("Stubs", `No local stubs found; downloading latest default stubs: ${route.revision}`);
    if (await this.downloadAndExtract(route)) {
      return this.configureRevision(route.revision, "default");
    }
    logWarn("Stubs", `Failed to download default stubs: ${route.revision}`);
    return null;
  }
  async ensureBoardStubs(boardRevision) {
    const revision = normalizeFirmwareRevision(boardRevision);
    if (!revision) {
      logWarn("Stubs", "Board revision unavailable; keeping default stubs");
      return this.ensureDefaultStubs();
    }
    this.boardRevisionRequested = revision;
    if (this.isCacheUsable(revision)) {
      logInfo("Stubs", `Using exact local stubs for connected board: ${revision}`);
      return this.configureRevision(revision, "board");
    }
    if (!this.canAutoDownload()) {
      logWarn("Stubs", `Exact board stubs are not cached and auto-download is disabled: ${revision}`);
      return null;
    }
    const route = await this.routeService.resolve(revision);
    if (!route) {
      logWarn("Stubs", `Unable to resolve stubs for connected board: ${revision}`);
      return null;
    }
    if (!route.exact) {
      logWarn("Stubs", `Exact board resources unavailable; using latest firmware resources: ${route.revision}`);
    }
    if (this.isCacheUsable(route.revision)) {
      logInfo("Stubs", `Using ${route.exact ? "exact" : "latest"} local stubs for connected board: ${route.revision}`);
      return this.configureRevision(route.revision, "board");
    }
    logInfo("Stubs", `Downloading ${route.exact ? "exact" : "latest"} stubs for connected board: ${route.revision}`);
    if (await this.downloadAndExtract(route)) {
      return this.configureRevision(route.revision, "board");
    }
    logWarn("Stubs", `Board stubs unavailable; keeping current default stubs: ${route.revision}`);
    return null;
  }
  async downloadStubs(boardRevision) {
    return boardRevision ? this.ensureBoardStubs(boardRevision) : this.ensureDefaultStubs();
  }
  async ensureRouteStubs(route, source) {
    const routeLabel = source === "default" ? "default" : route.exact ? "exact" : "latest";
    if (source === "board") {
      this.boardRevisionRequested = route.requestedRevision || route.revision;
      if (!route.exact) {
        logWarn("Stubs", `Exact board resources unavailable; using latest firmware resources: ${route.revision}`);
      }
    }
    if (this.isCacheUsable(route.revision)) {
      logInfo("Stubs", `Using ${routeLabel} local stubs: ${route.revision}`);
      return this.configureRevision(route.revision, source);
    }
    if (!this.canAutoDownload()) {
      logWarn("Stubs", `Stubs are not cached and auto-download is disabled: ${route.revision}`);
      return null;
    }
    logInfo("Stubs", `Downloading ${routeLabel} stubs: ${route.revision}`);
    if (await this.downloadAndExtract(route)) {
      return this.configureRevision(route.revision, source);
    }
    logWarn("Stubs", `Stubs unavailable: ${route.revision}`);
    return null;
  }
  canAutoDownload() {
    const autoDownload = vscode11.workspace.getConfiguration("canmv").get("stubsAutoDownload", true);
    if (!autoDownload) {
      logInfo("Stubs", "Auto-download disabled (canmv.stubsAutoDownload = false)");
      return false;
    }
    return true;
  }
  cacheDirFor(revision) {
    return path6.join(this.baseDir, revision);
  }
  isCacheUsable(revision) {
    const normalized = normalizeFirmwareRevision(revision);
    if (!normalized) return false;
    const cacheDir = this.cacheDirFor(normalized);
    return this.validateStubCache(cacheDir).ok;
  }
  validateStubCache(cacheDir) {
    try {
      if (!fs6.statSync(cacheDir).isDirectory()) {
        return { ok: false, pyiFiles: 0 };
      }
      const stats = this.collectStubCacheStats(cacheDir);
      return {
        ok: stats.pyiFiles > 0,
        pyiFiles: stats.pyiFiles
      };
    } catch {
      return { ok: false, pyiFiles: 0 };
    }
  }
  collectStubCacheStats(dir) {
    const entries = fs6.readdirSync(dir, { withFileTypes: true });
    let files = 0;
    let pyiFiles = 0;
    let maxMtimeMs = 0;
    for (const entry of entries) {
      const entryPath = path6.join(dir, entry.name);
      try {
        maxMtimeMs = Math.max(maxMtimeMs, fs6.statSync(entryPath).mtimeMs);
      } catch {
      }
      if (entry.isDirectory()) {
        const child = this.collectStubCacheStats(entryPath);
        files += child.files;
        pyiFiles += child.pyiFiles;
        maxMtimeMs = Math.max(maxMtimeMs, child.maxMtimeMs);
        continue;
      }
      if (entry.isFile() || entry.isSymbolicLink()) {
        files += 1;
        if (entry.name.endsWith(".pyi")) {
          pyiFiles += 1;
        }
      }
    }
    return { files, pyiFiles, maxMtimeMs };
  }
  stubCacheValidationMessage(validation) {
    if (validation.pyiFiles === 0) {
      return "no .pyi files found";
    }
    return "unknown validation failure";
  }
  findLatestLocalRevision() {
    try {
      if (!fs6.existsSync(this.baseDir)) return "";
      const revisions = fs6.readdirSync(this.baseDir, { withFileTypes: true }).filter((entry) => entry.isDirectory() && this.isCacheUsable(entry.name)).map((entry) => {
        const revision = entry.name;
        const mtime = fs6.statSync(this.cacheDirFor(revision)).mtimeMs;
        return { revision, mtime };
      }).sort((a, b) => b.mtime - a.mtime);
      return revisions[0]?.revision || "";
    } catch {
      return "";
    }
  }
  async configureRevision(revision, source) {
    const normalized = normalizeFirmwareRevision(revision);
    if (!this.isCacheUsable(normalized)) return null;
    if (source === "default" && this.boardRevisionRequested) {
      logInfo("Stubs", `Board-specific stubs requested; skipping default stubs switch: ${this.boardRevisionRequested}`);
      return null;
    }
    const cacheDir = this.cacheDirFor(normalized);
    if (!await this.configurePylance(cacheDir)) {
      return null;
    }
    await this.context?.globalState.update(_StubsService.lastRevisionKey, normalized);
    logInfo("Stubs", `Active ${source} stubs: ${normalized} (${cacheDir})`);
    return cacheDir;
  }
  async downloadAndExtract(route) {
    const ok = await vscode11.window.withProgress(
      {
        location: vscode11.ProgressLocation.Notification,
        title: t("CanMV: Downloading code completion stubs ({revision})...", { revision: route.revision })
      },
      () => this.performDownloadAndExtract(route)
    );
    if (!ok) {
      void vscode11.window.showWarningMessage(
        t("CanMV: Failed to download code completion stubs ({revision}). See the CanMV output for details.", { revision: route.revision })
      );
    }
    return ok;
  }
  async performDownloadAndExtract(route) {
    const cacheDir = this.cacheDirFor(route.revision);
    let tempDir = "";
    logInfo("Stubs", `Downloading stubs archive: ${route.stubsUrl}`);
    try {
      const data = await this.routeService.fetchBuffer(route.stubsUrl);
      if (!data || data.length === 0) {
        logWarn("Stubs", `Empty response from stubs archive: ${route.revision}`);
        return false;
      }
      tempDir = this.createCacheTempDir(route.revision);
      const zipPath = path6.join(tempDir, "stubs.zip");
      fs6.writeFileSync(zipPath, data);
      await this.extractArchive(zipPath, tempDir);
      fs6.unlinkSync(zipPath);
      this.flattenIfNeeded(tempDir);
      const validation = this.validateStubCache(tempDir);
      if (validation.ok) {
        this.replaceCacheDir(tempDir, cacheDir);
        logInfo("Stubs", `Extracted stubs archive: ${data.length} bytes, ${validation.pyiFiles} .pyi files -> ${cacheDir}`);
        return true;
      }
      this.cleanupCacheTempDir(tempDir);
      logWarn("Stubs", `Extracted archive is incomplete for ${route.revision}: ${this.stubCacheValidationMessage(validation)}`);
      return false;
    } catch (err) {
      logError("Stubs", `Download/extract failed for ${route.revision}: ${err}`);
      this.cleanupCacheTempDir(tempDir);
      return false;
    }
  }
  async configurePylance(stubsDir) {
    const workspace9 = this.firstFileWorkspaceFolder();
    const config = vscode11.workspace.getConfiguration("python.analysis", workspace9?.uri);
    const currentExtraPaths = config.get("extraPaths") || [];
    const currentStubPath = config.get("stubPath") || "";
    const currentDiagnosticOverrides = config.get("diagnosticSeverityOverrides") || {};
    const userStubPath = await this.resolveUserStubPath(currentStubPath);
    let overlayStubPath;
    let overlayRefreshed = false;
    try {
      const overlay = this.buildPylanceStubOverlay(stubsDir, userStubPath);
      overlayStubPath = overlay.stubPath;
      overlayRefreshed = overlay.refreshed;
    } catch (err) {
      logWarn("Stubs", `Could not build Pylance stub overlay; using stubs directory directly: ${err instanceof Error ? err.message : String(err)}`);
      overlayStubPath = stubsDir;
    }
    const nextExtraPaths = this.replaceCanMVStubsPath(currentExtraPaths, stubsDir);
    const nextDiagnosticOverrides = {
      ...currentDiagnosticOverrides,
      reportMissingModuleSource: "none"
    };
    const extraPathsChanged = !this.stringArraysEqual(currentExtraPaths, nextExtraPaths);
    const stubPathChanged = !this.pathsEqual(currentStubPath, overlayStubPath);
    const diagnosticsChanged = currentDiagnosticOverrides.reportMissingModuleSource !== "none";
    if (!extraPathsChanged && !stubPathChanged && !diagnosticsChanged) {
      logInfo("Stubs", `Pylance stubs already configured: ${overlayStubPath}`);
      const pylanceReady = await this.ensurePylanceExtensionReady();
      if (overlayRefreshed && pylanceReady) {
        await this.showPylanceReloadPrompt(stubsDir, overlayStubPath);
      }
      return true;
    }
    const targets = workspace9 ? [vscode11.ConfigurationTarget.Workspace] : [vscode11.ConfigurationTarget.Global];
    for (const target of targets) {
      try {
        if (extraPathsChanged) {
          await config.update("extraPaths", nextExtraPaths, target);
        }
        if (stubPathChanged) {
          await config.update("stubPath", overlayStubPath, target);
        }
        if (diagnosticsChanged) {
          await config.update("diagnosticSeverityOverrides", nextDiagnosticOverrides, target);
        }
        const scope = target === vscode11.ConfigurationTarget.Workspace ? "workspace" : "global";
        logInfo("Stubs", `Pylance python.analysis.stubPath configured (${scope}): ${overlayStubPath}`);
        const pylanceReady = await this.ensurePylanceExtensionReady();
        if (pylanceReady) {
          await this.showPylanceReloadPrompt(stubsDir, overlayStubPath);
        }
        return true;
      } catch (err) {
        logWarn("Stubs", `Could not update Pylance settings (${target}): ${err instanceof Error ? err.message : String(err)}`);
      }
    }
    logError("Stubs", `Failed to configure Pylance stubs path: ${overlayStubPath}`);
    void vscode11.window.showErrorMessage(
      t("CanMV: Failed to update Pylance settings. See the CanMV output for details.")
    );
    return false;
  }
  async ensurePylanceExtensionReady() {
    const pylance = vscode11.extensions.getExtension(_StubsService.pylanceExtensionId);
    if (!pylance) {
      logWarn("Stubs", `Pylance extension is not available in this VS Code extension host: ${_StubsService.pylanceExtensionId}`);
      this.showPylanceWarning(
        t("CanMV: Pylance is not available in this VS Code host. Install or enable Pylance for code completion tips.")
      );
      return false;
    }
    if (pylance.isActive) {
      logInfo("Stubs", `Pylance extension active: ${pylance.id}`);
      return true;
    }
    try {
      await pylance.activate();
      logInfo("Stubs", `Pylance extension activated: ${pylance.id}`);
      return true;
    } catch (err) {
      logWarn("Stubs", `Could not activate Pylance extension: ${err instanceof Error ? err.message : String(err)}`);
      this.showPylanceWarning(
        t("CanMV: Pylance could not activate in this VS Code host. Code completion tips may not work.")
      );
      return false;
    }
  }
  showPylanceWarning(message) {
    if (this.pylanceWarningShown) return;
    this.pylanceWarningShown = true;
    const openExtensions = t("Open Extensions");
    void vscode11.window.showWarningMessage(message, openExtensions).then((choice) => {
      if (choice === openExtensions) {
        void vscode11.commands.executeCommand(
          "workbench.extensions.search",
          `@id:${_StubsService.pylanceExtensionId}`
        );
      }
    });
  }
  async showPylanceReloadPrompt(stubsDir, overlayStubPath) {
    const signature = crypto.createHash("sha256").update([
      this.normalizeFsPathForCompare(stubsDir),
      this.normalizeFsPathForCompare(overlayStubPath),
      this.stubCacheSignature(stubsDir)
    ].join("\n")).digest("hex");
    const savedSignature = this.context?.workspaceState.get(_StubsService.reloadPromptSignatureKey) || "";
    if (signature === this.reloadPromptSignature || signature === savedSignature) {
      logInfo("Stubs", "Pylance reload prompt already shown for the current stubs configuration");
      return;
    }
    this.reloadPromptSignature = signature;
    await this.context?.workspaceState.update(_StubsService.reloadPromptSignatureKey, signature);
    const reloadAction = t("Reload Window");
    void vscode11.window.showInformationMessage(
      t("CanMV: Pylance stubs configured. Reload window for full effect."),
      reloadAction
    ).then((choice) => {
      if (choice === reloadAction) {
        void vscode11.commands.executeCommand("workbench.action.reloadWindow");
      }
    });
  }
  replaceCanMVStubsPath(extraPaths, stubsDir) {
    const next = [];
    let inserted = false;
    let removed = 0;
    for (const entry of extraPaths) {
      if (this.isCanMVStubsPath(entry)) {
        removed += 1;
        if (!inserted) {
          next.push(stubsDir);
          inserted = true;
        }
        continue;
      }
      next.push(entry);
    }
    if (!inserted && !next.some((entry) => this.pathsEqual(entry, stubsDir))) {
      next.push(stubsDir);
    }
    if (removed > 1) {
      logInfo("Stubs", `Collapsed ${removed} CanMV python.analysis.extraPaths entries into the current stubs path`);
    }
    return next;
  }
  async resolveUserStubPath(currentStubPath) {
    const savedStubPath = this.context?.workspaceState.get(_StubsService.userStubPathKey) || "";
    if (!currentStubPath || this.isCanMVManagedStubPath(currentStubPath)) {
      return this.usableStubRoot(savedStubPath);
    }
    const resolved = this.usableStubRoot(currentStubPath);
    if (!resolved || this.isCanMVStubsPath(resolved) || this.isCanMVOverlayPath(resolved)) {
      await this.context?.workspaceState.update(_StubsService.userStubPathKey, void 0);
      return "";
    }
    await this.context?.workspaceState.update(_StubsService.userStubPathKey, currentStubPath);
    return resolved;
  }
  buildPylanceStubOverlay(stubsDir, userStubPath) {
    const overlayDir = this.pylanceOverlayDir();
    const cacheSignature = this.stubCacheSignature(stubsDir);
    const userStubSignature = userStubPath ? this.stubCacheSignature(userStubPath) : "";
    if (this.isPylanceStubOverlayCurrent(overlayDir, stubsDir, userStubPath, cacheSignature, userStubSignature)) {
      return { stubPath: overlayDir, refreshed: false };
    }
    const tempDir = this.createPylanceOverlayTempDir(overlayDir);
    try {
      fs6.mkdirSync(tempDir, { recursive: true });
      this.copyStubRoot(stubsDir, tempDir, true);
      if (userStubPath) {
        this.copyStubRoot(userStubPath, tempDir, false);
      }
      if (!this.isStubRootCopied(stubsDir, tempDir) || userStubPath && !this.isMergedUserStubRootCopied(userStubPath, stubsDir, tempDir)) {
        throw new Error("Pylance stub overlay verification failed");
      }
      this.writePylanceStubOverlayManifest(tempDir, stubsDir, userStubPath, cacheSignature, userStubSignature);
      this.replacePylanceStubOverlay(tempDir, overlayDir);
      return { stubPath: overlayDir, refreshed: true };
    } catch (err) {
      this.cleanupPylanceOverlayTempDir(tempDir);
      throw err;
    }
  }
  isPylanceStubOverlayCurrent(overlayDir, stubsDir, userStubPath, cacheSignature, userStubSignature) {
    try {
      if (!fs6.statSync(overlayDir).isDirectory()) {
        return false;
      }
      const manifestPath = path6.join(overlayDir, _StubsService.overlayManifestFile);
      const manifest = JSON.parse(fs6.readFileSync(manifestPath, "utf8"));
      const manifestMatches = manifest.version === 2 && this.pathsEqual(manifest.stubsDir || "", stubsDir) && this.pathsEqual(manifest.userStubPath || "", userStubPath) && manifest.cacheSignature === cacheSignature && manifest.userStubSignature === userStubSignature;
      return manifestMatches && this.isStubRootCopied(stubsDir, overlayDir) && (!userStubPath || this.isMergedUserStubRootCopied(userStubPath, stubsDir, overlayDir));
    } catch {
      return false;
    }
  }
  writePylanceStubOverlayManifest(overlayDir, stubsDir, userStubPath, cacheSignature, userStubSignature) {
    const manifest = {
      version: 2,
      stubsDir,
      userStubPath,
      cacheSignature,
      userStubSignature
    };
    fs6.writeFileSync(
      path6.join(overlayDir, _StubsService.overlayManifestFile),
      `${JSON.stringify(manifest, null, 2)}
`
    );
  }
  stubCacheSignature(stubsDir) {
    const stats = this.collectStubCacheStats(stubsDir);
    return `${stats.files}:${stats.pyiFiles}:${Math.trunc(stats.maxMtimeMs)}`;
  }
  createCacheTempDir(revision) {
    fs6.mkdirSync(this.baseDir, { recursive: true });
    return fs6.mkdtempSync(path6.join(this.baseDir, `${revision}.tmp-`));
  }
  replaceCacheDir(tempDir, cacheDir) {
    if (!this.isCanMVStubsPath(tempDir)) {
      throw new Error(`Refusing to use non-CanMV stubs temp path: ${tempDir}`);
    }
    if (!this.isCanMVStubsPath(cacheDir)) {
      throw new Error(`Refusing to replace non-CanMV stubs cache path: ${cacheDir}`);
    }
    fs6.rmSync(cacheDir, { recursive: true, force: true });
    fs6.renameSync(tempDir, cacheDir);
  }
  cleanupCacheTempDir(tempDir) {
    if (!tempDir) return;
    try {
      if (this.isCanMVStubsPath(tempDir)) {
        fs6.rmSync(tempDir, { recursive: true, force: true });
      }
    } catch (err) {
      logWarn("Stubs", `Could not remove temporary stubs cache: ${err instanceof Error ? err.message : String(err)}`);
    }
  }
  createPylanceOverlayTempDir(overlayDir) {
    const parentDir = path6.dirname(overlayDir);
    fs6.mkdirSync(parentDir, { recursive: true });
    return fs6.mkdtempSync(path6.join(parentDir, "typings-"));
  }
  replacePylanceStubOverlay(tempDir, overlayDir) {
    if (!this.isCanMVOverlayPath(overlayDir)) {
      throw new Error(`Refusing to replace non-CanMV Pylance overlay path: ${overlayDir}`);
    }
    if (!this.isCanMVOverlayPath(tempDir)) {
      throw new Error(`Refusing to use non-CanMV Pylance overlay temp path: ${tempDir}`);
    }
    try {
      fs6.rmSync(overlayDir, { recursive: true, force: true });
    } catch (err) {
      throw new Error(`Could not reset Pylance overlay: ${err instanceof Error ? err.message : String(err)}`);
    }
    fs6.renameSync(tempDir, overlayDir);
  }
  cleanupPylanceOverlayTempDir(tempDir) {
    try {
      if (this.isCanMVOverlayPath(tempDir)) {
        fs6.rmSync(tempDir, { recursive: true, force: true });
      }
    } catch (err) {
      logWarn("Stubs", `Could not remove temporary Pylance overlay: ${err instanceof Error ? err.message : String(err)}`);
    }
  }
  copyStubRoot(sourceDir, overlayDir, overwrite) {
    const normalizedSource = this.normalizeFsPathForCompare(sourceDir);
    const normalizedOverlay = this.normalizeFsPathForCompare(overlayDir);
    if (!normalizedSource || normalizedSource === normalizedOverlay) return;
    try {
      const entries = fs6.readdirSync(sourceDir, { withFileTypes: true });
      for (const entry of entries) {
        const source = path6.join(sourceDir, entry.name);
        const target = path6.join(overlayDir, entry.name);
        if (fs6.existsSync(target)) {
          if (!overwrite) continue;
          fs6.rmSync(target, { recursive: true, force: true });
        }
        fs6.cpSync(source, target, { recursive: true, dereference: true, errorOnExist: true, force: false });
      }
    } catch (err) {
      throw new Error(`Could not copy stub root into Pylance overlay: ${sourceDir}: ${err instanceof Error ? err.message : String(err)}`);
    }
  }
  isStubRootCopied(sourceDir, overlayDir) {
    try {
      for (const entry of fs6.readdirSync(sourceDir, { withFileTypes: true })) {
        const source = path6.join(sourceDir, entry.name);
        const target = path6.join(overlayDir, entry.name);
        const sourceStat = fs6.statSync(source);
        const targetStat = fs6.lstatSync(target);
        if (targetStat.isSymbolicLink()) {
          return false;
        }
        if (sourceStat.isDirectory()) {
          if (!targetStat.isDirectory() || !this.isStubRootCopied(source, target)) {
            return false;
          }
        } else if (!targetStat.isFile() || sourceStat.size !== targetStat.size) {
          return false;
        }
      }
      return true;
    } catch {
      return false;
    }
  }
  isMergedUserStubRootCopied(userStubPath, stubsDir, overlayDir) {
    try {
      const canmvEntries = new Set(fs6.readdirSync(stubsDir));
      for (const entry of fs6.readdirSync(userStubPath, { withFileTypes: true })) {
        if (canmvEntries.has(entry.name)) {
          continue;
        }
        const source = path6.join(userStubPath, entry.name);
        const target = path6.join(overlayDir, entry.name);
        if (!this.isStubEntryCopied(source, target)) {
          return false;
        }
      }
      return true;
    } catch {
      return false;
    }
  }
  isStubEntryCopied(source, target) {
    try {
      const sourceStat = fs6.statSync(source);
      const targetStat = fs6.lstatSync(target);
      if (targetStat.isSymbolicLink()) {
        return false;
      }
      if (sourceStat.isDirectory()) {
        return targetStat.isDirectory() && this.isStubRootCopied(source, target);
      }
      return targetStat.isFile() && sourceStat.size === targetStat.size;
    } catch {
      return false;
    }
  }
  pylanceOverlayDir() {
    const workspace9 = this.firstFileWorkspaceFolder();
    const scope = workspace9?.uri.toString() || this.context?.globalStorageUri.toString() || os2.homedir();
    const scopeHash = crypto.createHash("sha256").update(scope).digest("hex").slice(0, 16);
    return path6.join(this.pylanceOverlayBaseDir, scopeHash, "typings");
  }
  firstFileWorkspaceFolder() {
    return vscode11.workspace.workspaceFolders?.find((folder) => folder.uri.scheme === "file");
  }
  usableStubRoot(value) {
    if (!value) return "";
    const resolved = this.resolveConfiguredPath(value);
    if (!resolved) return "";
    try {
      return fs6.statSync(resolved).isDirectory() ? resolved : "";
    } catch {
      return "";
    }
  }
  resolveConfiguredPath(value) {
    const expanded = this.expandHome(value.trim());
    if (!expanded) return "";
    if (path6.isAbsolute(expanded)) {
      return path6.resolve(path6.normalize(expanded));
    }
    const workspace9 = this.firstFileWorkspaceFolder();
    if (!workspace9) return "";
    return path6.resolve(workspace9.uri.fsPath, path6.normalize(expanded));
  }
  isCanMVManagedStubPath(value) {
    return this.isCanMVStubsPath(value) || this.isCanMVOverlayPath(value);
  }
  isCanMVStubsPath(value) {
    if (!value) return false;
    if (this.hasCanMVManagedPathMarker(value, "k230_canmv_stubs")) return true;
    const baseDir = this.normalizeFsPathForCompare(this.baseDir);
    const candidate = this.normalizeFsPathForCompare(value);
    const relative3 = path6.relative(baseDir, candidate);
    return relative3 === "" || !!relative3 && !relative3.startsWith("..") && !path6.isAbsolute(relative3);
  }
  isCanMVOverlayPath(value) {
    if (!value) return false;
    if (this.hasCanMVManagedPathMarker(value, "k230_canmv_pylance")) return true;
    const baseDir = this.normalizeFsPathForCompare(this.pylanceOverlayBaseDir);
    const candidate = this.normalizeFsPathForCompare(value);
    const relative3 = path6.relative(baseDir, candidate);
    return relative3 === "" || !!relative3 && !relative3.startsWith("..") && !path6.isAbsolute(relative3);
  }
  hasCanMVManagedPathMarker(value, directoryName) {
    const normalized = this.expandHome(value).replace(/\\/g, "/").replace(/\/+/g, "/").trim();
    const marker = `/.kendryte/${directoryName}`;
    return normalized.endsWith(marker) || normalized.includes(`${marker}/`) || normalized === `.kendryte/${directoryName}` || normalized.startsWith(`.kendryte/${directoryName}/`);
  }
  pathsEqual(left, right) {
    return this.normalizeFsPathForCompare(left) === this.normalizeFsPathForCompare(right);
  }
  normalizeFsPathForCompare(value) {
    const expanded = this.expandHome(value);
    const normalized = path6.resolve(path6.normalize(expanded));
    return process.platform === "win32" ? normalized.toLowerCase() : normalized;
  }
  expandHome(value) {
    const trimmed = value.trim();
    return trimmed === "~" ? os2.homedir() : trimmed.startsWith("~/") || trimmed.startsWith("~\\") ? path6.join(os2.homedir(), trimmed.slice(2)) : trimmed;
  }
  stringArraysEqual(left, right) {
    return left.length === right.length && left.every((value, index) => value === right[index]);
  }
  async extractArchive(archivePath, targetDir) {
    if (!this.context) {
      throw new Error("CanMV backend unavailable for stubs archive extraction");
    }
    const backend2 = resolveNativeBackendCommand(this.context);
    await new Promise((resolve3, reject) => {
      (0, import_child_process.execFile)(
        backend2.command,
        [...backend2.args, "--extract-archive", archivePath, targetDir],
        { cwd: backend2.cwd, windowsHide: true, timeout: 6e4 },
        (err, stdout, stderr) => {
          if (err) {
            const detail = stderr?.trim() || stdout?.trim() || err.message;
            reject(new Error(detail));
            return;
          }
          resolve3();
        }
      );
    });
  }
  flattenIfNeeded(targetDir) {
    try {
      const entries = fs6.readdirSync(targetDir, { withFileTypes: true });
      const pyis = entries.filter((e) => e.isFile() && e.name.endsWith(".pyi"));
      if (pyis.length > 0) return true;
      const subdirs = entries.filter((e) => e.isDirectory());
      for (const sub of subdirs) {
        const subPath = path6.join(targetDir, sub.name);
        const subEntries = fs6.readdirSync(subPath, { withFileTypes: true });
        const subPyis = subEntries.filter((e) => e.isFile() && e.name.endsWith(".pyi"));
        if (subPyis.length > 0) {
          for (const entry of subEntries) {
            fs6.renameSync(path6.join(subPath, entry.name), path6.join(targetDir, entry.name));
          }
          try {
            fs6.rmdirSync(subPath);
          } catch {
          }
          return true;
        }
      }
    } catch {
    }
    return false;
  }
};

// src/service/canmvResourceService.ts
var CanmvResourceService = class {
  constructor(routeService, stubsService2, examplesService2) {
    this.routeService = routeService;
    this.stubsService = stubsService2;
    this.examplesService = examplesService2;
  }
  async ensureDefaultResources() {
    const route = await this.routeService.resolve("");
    if (route) {
      await this.examplesService.ensureExamples(route);
      return this.stubsService.ensureRouteStubs(route, "default");
    }
    logWarn("Resources", "Failed to resolve latest CanMV resources; using local cached resources");
    await this.examplesService.ensureExamples(null);
    return this.stubsService.ensureDefaultStubs();
  }
  async ensureDefaultExamples() {
    const route = await this.routeService.resolve("");
    return this.examplesService.ensureExamples(route);
  }
  async ensureBoardResources(boardRevision) {
    const route = await this.routeService.resolve(boardRevision);
    if (!route) {
      await this.examplesService.ensureExamples(null);
      const stubsPath = await this.stubsService.downloadStubs(boardRevision);
      return stubsPath;
    }
    await this.examplesService.ensureExamples(route);
    return this.stubsService.ensureRouteStubs(route, "board");
  }
};

// src/service/remoteMirrorService.ts
var path7 = __toESM(require("path"));
var os3 = __toESM(require("os"));
var crypto2 = __toESM(require("crypto"));
var vscode12 = __toESM(require("vscode"));
var CANMV_SCHEME = "canmv";
var MIRROR_DIR = "canmv-vscode";
var REMOTE_DIR = "remote";
var PYTHON_EXTENSIONS = /* @__PURE__ */ new Set([".py", ".pyi"]);
var COMMON_IMPORT_ROOTS = ["sdcard", "data", "udisk"];
var RemoteMirrorService = class {
  constructor(context, fileService, isRemoteAvailable = () => true, unavailableMessage = () => "Remote files are not available") {
    this.context = context;
    this.fileService = fileService;
    this.isRemoteAvailable = isRemoteAvailable;
    this.unavailableMessage = unavailableMessage;
    this.localToRemote = /* @__PURE__ */ new Map();
  }
  async openRemoteFile(remotePath) {
    this.assertAvailable();
    const normalizedRemotePath = normalizeRemotePath2(remotePath);
    if (!isPythonFile(normalizedRemotePath)) {
      await vscode12.commands.executeCommand("vscode.open", vscode12.Uri.from({
        scheme: CANMV_SCHEME,
        path: normalizedRemotePath
      }));
      return;
    }
    const localUri = this.localUriForRemotePath(normalizedRemotePath);
    this.remember(localUri, normalizedRemotePath);
    const openDocument = this.findOpenDocument(localUri);
    if (!openDocument?.isDirty) {
      const data = await this.fileService.readFile(normalizedRemotePath);
      await vscode12.workspace.fs.createDirectory(parentUri(localUri));
      await vscode12.workspace.fs.writeFile(localUri, data);
      logInfo("Mirror", `Synced ${normalizedRemotePath} -> ${localUri.fsPath}`);
    }
    await this.ensurePythonAnalysisPaths(localUri, normalizedRemotePath);
    const document = await vscode12.workspace.openTextDocument(localUri);
    await vscode12.window.showTextDocument(document, { preview: false });
  }
  remotePathForDocument(document) {
    return this.remotePathForUri(document.uri);
  }
  remotePathForUri(uri) {
    if (uri.scheme !== "file") return void 0;
    const key = normalizeFsPath(uri.fsPath);
    const mapped = this.localToRemote.get(key);
    if (mapped) return mapped;
    const relativePath = path7.relative(this.mirrorRootUri().fsPath, uri.fsPath);
    if (!relativePath || relativePath.startsWith("..") || path7.isAbsolute(relativePath)) {
      return void 0;
    }
    return normalizeRemotePath2("/" + relativePath.split(path7.sep).join("/"));
  }
  async syncDocumentToRemote(document) {
    const remotePath = this.remotePathForDocument(document);
    if (!remotePath) return false;
    this.assertAvailable();
    const data = new TextEncoder().encode(document.getText());
    const ok = await this.fileService.writeFile(remotePath, data);
    if (ok) {
      logInfo("Mirror", `Synced ${document.uri.fsPath} -> ${remotePath}`);
    } else {
      logWarn("Mirror", `Write rejected for mirrored file: ${remotePath}`);
    }
    return ok;
  }
  localUriForRemotePath(remotePath) {
    const parts = normalizeRemotePath2(remotePath).split("/").filter(Boolean).map(sanitizePathSegment);
    return vscode12.Uri.joinPath(this.mirrorRootUri(), ...parts);
  }
  mirrorRootUri() {
    const workspace9 = vscode12.workspace.workspaceFolders?.find((folder) => folder.uri.scheme === "file");
    const scope = workspace9?.uri.toString() || this.context.globalStorageUri.toString();
    const scopeHash = crypto2.createHash("sha256").update(scope).digest("hex").slice(0, 16);
    const userDir = `${MIRROR_DIR}-${os3.userInfo().username}`;
    return vscode12.Uri.file(path7.join(os3.tmpdir(), userDir, scopeHash, REMOTE_DIR));
  }
  findOpenDocument(uri) {
    const target = normalizeFsPath(uri.fsPath);
    return vscode12.workspace.textDocuments.find(
      (document) => document.uri.scheme === "file" && normalizeFsPath(document.uri.fsPath) === target
    );
  }
  remember(localUri, remotePath) {
    this.localToRemote.set(normalizeFsPath(localUri.fsPath), normalizeRemotePath2(remotePath));
  }
  async ensurePythonAnalysisPaths(localUri, remotePath) {
    const workspace9 = vscode12.workspace.workspaceFolders?.find((folder) => folder.uri.scheme === "file");
    if (!workspace9) return;
    const mirrorRoot = this.mirrorRootUri().fsPath;
    const topLevel = normalizeRemotePath2(remotePath).split("/").filter(Boolean)[0];
    const candidatePaths = [
      mirrorRoot,
      ...COMMON_IMPORT_ROOTS.map((root) => path7.join(mirrorRoot, root)),
      topLevel ? path7.join(mirrorRoot, sanitizePathSegment(topLevel)) : "",
      path7.dirname(localUri.fsPath)
    ].filter(Boolean);
    const config = vscode12.workspace.getConfiguration("python.analysis", workspace9.uri);
    const current = config.get("extraPaths") || [];
    const currentKeys = new Set(current.map(normalizeFsPath));
    const next = [...current];
    for (const candidate of candidatePaths) {
      const key = normalizeFsPath(candidate);
      if (!currentKeys.has(key)) {
        currentKeys.add(key);
        next.push(candidate);
      }
    }
    if (next.length === current.length) return;
    try {
      await config.update("extraPaths", next, vscode12.ConfigurationTarget.Workspace);
      logInfo("Mirror", `Pylance extraPaths updated for CanMV mirror: ${mirrorRoot}`);
    } catch (err) {
      logWarn("Mirror", `Could not update python.analysis.extraPaths: ${err instanceof Error ? err.message : String(err)}`);
    }
  }
  assertAvailable() {
    if (!this.isRemoteAvailable()) {
      throw new Error(this.unavailableMessage());
    }
  }
};
function parentUri(uri) {
  return vscode12.Uri.file(path7.dirname(uri.fsPath));
}
function normalizeRemotePath2(value) {
  const pathValue = value.trim() || "/";
  return ("/" + pathValue.replace(/^\/+/, "")).replace(/\/+$/g, "") || "/";
}
function normalizeFsPath(value) {
  return path7.normalize(value);
}
function sanitizePathSegment(value) {
  if (!value || value === "." || value === "..") return "_";
  return value.replace(/[<>:"\\|?*\x00-\x1F]/g, "_");
}
function isPythonFile(remotePath) {
  return PYTHON_EXTENSIONS.has(path7.extname(remotePath).toLowerCase());
}

// src/explorer/treeProvider.ts
var vscode14 = __toESM(require("vscode"));

// src/explorer/fileItem.ts
var vscode13 = __toESM(require("vscode"));
var SIZE_UNITS = ["B", "KB", "MB", "GB"];
var FileTreeItem = class _FileTreeItem extends vscode13.TreeItem {
  constructor(name, fileType, absPath, size = 0) {
    super(
      name,
      fileType === "directory" ? vscode13.TreeItemCollapsibleState.Collapsed : vscode13.TreeItemCollapsibleState.None
    );
    this.name = name;
    this.fileType = fileType;
    this.absPath = absPath;
    this.size = size;
    if (absPath) {
      this.id = absPath;
    }
    this.iconPath = new vscode13.ThemeIcon(
      fileType === "directory" ? "folder" : fileType === "message" ? "warning" : "file"
    );
    if (fileType === "file") {
      this.description = formatFileSize2(size);
      this.tooltip = `${absPath}
${t("{size} {unit}", { size, unit: size === 1 ? t("byte") : t("bytes") })}`;
      this.contextValue = name.endsWith(".py") ? "pythonFile" : "file";
      this.command = {
        command: "canmv.openRemoteFile",
        title: t("Open Remote File"),
        arguments: [this]
      };
    } else {
      this.tooltip = absPath;
      this.contextValue = fileType === "message" ? "message" : isMountRoot(absPath) ? "mountRoot" : "directory";
    }
  }
  static message(name) {
    return new _FileTreeItem(name, "message", "", 0);
  }
  setLoading(loading) {
    if (this.fileType === "directory") {
      this.iconPath = new vscode13.ThemeIcon(loading ? "sync~spin" : "folder");
    }
  }
};
function formatFileSize2(size) {
  if (!Number.isFinite(size) || size <= 0) return "0 B";
  let value = size;
  let unitIndex = 0;
  while (value >= 1024 && unitIndex < SIZE_UNITS.length - 1) {
    value /= 1024;
    unitIndex++;
  }
  if (unitIndex === 0) return `${size} B`;
  const digits = value < 10 ? 1 : 0;
  return `${value.toFixed(digits)} ${SIZE_UNITS[unitIndex]}`;
}
function isMountRoot(path15) {
  return path15 === "/sdcard" || path15 === "/data" || path15 === "/udisk";
}

// src/explorer/treeProvider.ts
var entryNameCollator = new Intl.Collator(void 0, {
  numeric: true,
  sensitivity: "base"
});
var CanmvExplorer = class {
  constructor(fileOps) {
    this.fileOps = fileOps;
    this._onDidChangeTreeData = new vscode14.EventEmitter();
    this.onDidChangeTreeData = this._onDidChangeTreeData.event;
    this.connected = false;
    this.fileExplorerSupported = true;
    this.unavailableMessage = t("Not connected");
    this.listingGeneration = 0;
    this.listings = /* @__PURE__ */ new Map();
    this.cancelledListingPaths = /* @__PURE__ */ new Set();
  }
  setConnected(connected) {
    this.setConnectionState(connected, true);
  }
  setConnectionState(connected, fileExplorerSupported, unavailableMessage = t("Not connected")) {
    const shouldRefresh = this.connected !== connected || this.fileExplorerSupported !== fileExplorerSupported || !connected && this.unavailableMessage !== unavailableMessage;
    this.connected = connected;
    this.fileExplorerSupported = fileExplorerSupported;
    this.unavailableMessage = unavailableMessage;
    if (shouldRefresh) {
      this.refresh();
    }
  }
  refresh() {
    this.listingGeneration++;
    this.listings.clear();
    this.cancelledListingPaths.clear();
    this.activeListing = void 0;
    this._onDidChangeTreeData.fire();
  }
  async getChildren(element) {
    if (!this.fileExplorerSupported) {
      return [FileTreeItem.message(t("File explorer is not supported by this firmware"))];
    }
    if (!this.connected) {
      return [FileTreeItem.message(this.unavailableMessage)];
    }
    if (!element) {
      return this.getDirectoryChildren("/", void 0, t("Error loading /"));
    }
    if (element.fileType === "directory") {
      return this.getDirectoryChildren(element.absPath, element, t("Error loading folder"));
    }
    return [];
  }
  getTreeItem(element) {
    element.setLoading(this.isDirectoryListingInProgress(element.absPath));
    return element;
  }
  resumeListing(element) {
    if (element.fileType !== "directory" || !this.cancelledListingPaths.delete(element.absPath)) {
      return;
    }
    this._onDidChangeTreeData.fire(element);
  }
  async getDirectoryChildren(path15, element, errorMessage5) {
    if (element && this.cancelledListingPaths.has(path15)) {
      return [];
    }
    const listing = this.getListing(path15);
    const operation = this.selectListing(path15, listing, element);
    if (listing.entries.length === 0 && !listing.complete) {
      try {
        await this.loadNextPage(path15, listing);
      } catch {
        if (operation && !this.isActiveListing(operation)) {
          return this.toTreeItems(path15, listing.entries);
        }
        return [FileTreeItem.message(errorMessage5)];
      }
    }
    if (operation && !listing.complete && this.isActiveListing(operation)) {
      void this.loadRemainingPages(operation, element);
    }
    const children = this.toTreeItems(path15, listing.entries);
    if (listing.error && (!operation || this.isActiveListing(operation))) {
      children.push(FileTreeItem.message(errorMessage5));
    }
    return children;
  }
  getListing(path15) {
    let listing = this.listings.get(path15);
    if (!listing) {
      listing = {
        entries: [],
        nextOffset: 0,
        complete: false,
        generation: this.listingGeneration
      };
      this.listings.set(path15, listing);
    }
    return listing;
  }
  selectListing(path15, listing, element) {
    if (this.activeListing && (this.activeListing.path !== path15 || this.activeListing.listing !== listing)) {
      this.cancelListing(this.activeListing);
    }
    if (listing.complete) {
      return void 0;
    }
    if (this.activeListing) {
      this.activeListing.element = element;
      return this.activeListing;
    }
    const operation = { path: path15, listing, element };
    this.activeListing = operation;
    return operation;
  }
  cancelListing(operation) {
    if (this.activeListing === operation) {
      this.activeListing = void 0;
    }
    operation.element?.setLoading(false);
    if (operation.listing.complete || this.listings.get(operation.path) !== operation.listing) {
      return;
    }
    operation.listing.entries.length = 0;
    operation.listing.nextOffset = void 0;
    operation.listing.error = void 0;
    this.listings.delete(operation.path);
    this.cancelledListingPaths.add(operation.path);
    if (operation.element) {
      this._onDidChangeTreeData.fire(operation.element);
    }
  }
  async loadNextPage(path15, listing) {
    if (listing.complete || listing.nextOffset === void 0) {
      listing.complete = true;
      return;
    }
    if (listing.pageLoad) {
      return listing.pageLoad;
    }
    const offset = listing.nextOffset;
    const load = (async () => {
      const page = await this.readPage(path15, offset);
      if (!this.isCurrentListing(path15, listing)) {
        return;
      }
      listing.entries.push(...page.entries);
      if (page.nextOffset === void 0) {
        listing.complete = true;
        listing.nextOffset = void 0;
        return;
      }
      if (!Number.isSafeInteger(page.nextOffset) || page.nextOffset <= offset || page.nextOffset > 4294967295) {
        throw new Error(`Invalid directory listing continuation for ${path15}`);
      }
      listing.nextOffset = page.nextOffset;
    })();
    listing.pageLoad = load;
    try {
      await load;
    } catch (error) {
      if (this.isCurrentListing(path15, listing)) {
        listing.complete = true;
        listing.error = error;
      }
      throw error;
    } finally {
      if (listing.pageLoad === load) {
        listing.pageLoad = void 0;
      }
    }
  }
  async loadRemainingPages(operation, element) {
    if (operation.backgroundLoad || operation.listing.complete || !this.isActiveListing(operation)) {
      return;
    }
    const load = this.loadRemainingPagesForOperation(operation, element);
    operation.backgroundLoad = load;
    this._onDidChangeTreeData.fire(element);
    try {
      await load;
    } finally {
      if (operation.backgroundLoad === load) {
        operation.backgroundLoad = void 0;
      }
    }
  }
  async loadRemainingPagesForOperation(operation, element) {
    try {
      while (this.isActiveListing(operation) && !operation.listing.complete) {
        await this.loadNextPage(operation.path, operation.listing);
        if (this.isActiveListing(operation)) {
          this._onDidChangeTreeData.fire(element);
        }
      }
    } catch {
      if (this.isActiveListing(operation)) {
        this._onDidChangeTreeData.fire(element);
      }
    }
  }
  async readPage(path15, offset) {
    if (this.fileOps.listDirPage) {
      return this.fileOps.listDirPage(path15, offset);
    }
    return { entries: await this.fileOps.listDir(path15) };
  }
  isCurrentListing(path15, listing) {
    return listing.generation === this.listingGeneration && this.listings.get(path15) === listing;
  }
  isActiveListing(operation) {
    return this.activeListing === operation && this.isCurrentListing(operation.path, operation.listing);
  }
  isDirectoryListingInProgress(path15) {
    const operation = this.activeListing;
    return !!operation && operation.path === path15 && !operation.listing.complete && this.isActiveListing(operation);
  }
  toTreeItems(path15, entries) {
    return sortEntries(entries).map((e) => new FileTreeItem(
      e.name,
      e.type,
      path15 === "/" ? "/" + e.name : path15 + "/" + e.name,
      e.size
    ));
  }
};
function sortEntries(entries) {
  return [...entries].sort((a, b) => {
    if (a.type !== b.type) {
      return a.type === "directory" ? -1 : 1;
    }
    return entryNameCollator.compare(a.name, b.name);
  });
}

// src/explorer/examplesTreeProvider.ts
var vscode15 = __toESM(require("vscode"));
var fs7 = __toESM(require("fs"));
var path8 = __toESM(require("path"));
var ExampleTreeItem = class _ExampleTreeItem extends vscode15.TreeItem {
  constructor(labelText, fsPath, isDirectory) {
    super(
      labelText,
      isDirectory ? vscode15.TreeItemCollapsibleState.Collapsed : vscode15.TreeItemCollapsibleState.None
    );
    this.labelText = labelText;
    this.fsPath = fsPath;
    this.isDirectory = isDirectory;
    this.tooltip = fsPath || labelText;
    this.iconPath = new vscode15.ThemeIcon(!fsPath ? "info" : isDirectory ? "folder" : this.iconForFile(labelText));
    this.contextValue = !fsPath ? "message" : isDirectory ? "localExampleDirectory" : this.isPythonFile(labelText) ? "localExamplePythonFile" : this.isTextFile(labelText) ? "localExampleTextFile" : "localExampleFile";
    if (fsPath && !isDirectory && this.isTextFile(labelText)) {
      this.command = {
        command: "canmv.openExampleFile",
        title: t("Open Example File"),
        arguments: [this]
      };
    }
  }
  static message(label) {
    return new _ExampleTreeItem(label, "", false);
  }
  iconForFile(name) {
    return name.endsWith(".py") ? "symbol-method" : "file";
  }
  isPythonFile(name) {
    return /\.py$/i.test(name);
  }
  isTextFile(name) {
    return /\.(py|txt|md|json|ya?ml|csv|ini|toml|cfg|conf|sh|c|h|cpp|hpp)$/i.test(name);
  }
};
var ExamplesTreeProvider = class {
  constructor(examplesService2) {
    this.examplesService = examplesService2;
    this._onDidChangeTreeData = new vscode15.EventEmitter();
    this.onDidChangeTreeData = this._onDidChangeTreeData.event;
    this.examplesService.onDidChangeExamples(() => this.refresh());
  }
  refresh() {
    this._onDidChangeTreeData.fire();
  }
  getTreeItem(element) {
    return element;
  }
  getChildren(element) {
    const root = element?.fsPath || this.examplesService.activeExamplesDir();
    if (!root) {
      return [ExampleTreeItem.message(t("No examples downloaded yet"))];
    }
    if (!fs7.existsSync(root)) {
      return [ExampleTreeItem.message(t("Examples cache not found"))];
    }
    try {
      return fs7.readdirSync(root, { withFileTypes: true }).filter((entry) => !entry.name.startsWith(".")).sort((a, b) => Number(b.isDirectory()) - Number(a.isDirectory()) || a.name.localeCompare(b.name)).map((entry) => {
        const fsPath = path8.join(root, entry.name);
        return new ExampleTreeItem(entry.name, fsPath, entry.isDirectory());
      });
    } catch (err) {
      return [ExampleTreeItem.message(t("Failed to read examples"))];
    }
  }
};

// src/backend/detector.ts
var BoardDetector = class {
  constructor(requester) {
    this.requester = requester;
  }
  async scan() {
    const result = await this.requester.request(createRequest(Methods.detectBoards, {}));
    if (isResponse(result)) {
      const payload = result.result;
      return Array.isArray(payload.boards) ? payload.boards : [];
    }
    logError("Board", `Detect failed: ${result.error.message}`);
    return [];
  }
};

// src/filesystem/provider.ts
var vscode16 = __toESM(require("vscode"));
var WRITABLE_ROOTS = /* @__PURE__ */ new Set(["sdcard", "data", "udisk"]);
var CanmvFileSystemProvider = class {
  constructor(fileService, availability = {
    isAvailable: () => true,
    unavailableMessage: () => t("Remote files are not available")
  }) {
    this.fileService = fileService;
    this.availability = availability;
    this._emitter = new vscode16.EventEmitter();
    this.onDidChangeFile = this._emitter.event;
  }
  async stat(uri) {
    if (uri.path === "/") {
      return { type: vscode16.FileType.Directory, ctime: 0, mtime: 0, size: 0 };
    }
    this.assertAvailable();
    const entry = await this.findEntry(uri);
    if (entry) {
      return {
        type: entry.type === "directory" ? vscode16.FileType.Directory : vscode16.FileType.File,
        ctime: 0,
        mtime: 0,
        size: entry.size || 0
      };
    }
    const r = await this.fileService.statFile(uri.path);
    if (!r.exists) {
      throw vscode16.FileSystemError.FileNotFound(uri);
    }
    return {
      type: r.type === "directory" ? vscode16.FileType.Directory : vscode16.FileType.File,
      ctime: 0,
      mtime: r.mtime ? r.mtime * 1e3 : 0,
      size: r.size
    };
  }
  async readDirectory(uri) {
    const entries = await this.listDir(uri.path);
    return entries.map((e) => [
      e.name,
      e.type === "directory" ? vscode16.FileType.Directory : vscode16.FileType.File
    ]);
  }
  async readFile(uri) {
    this.assertAvailable();
    try {
      return await this.fileService.readFile(uri.path);
    } catch (err) {
      throw vscode16.FileSystemError.FileNotFound(uri);
    }
  }
  async writeFile(uri, content, options) {
    this.assertAvailable();
    this.assertWritablePath(uri);
    const exists = await this.exists(uri);
    if (exists && !options.overwrite) {
      throw vscode16.FileSystemError.FileExists(uri);
    }
    if (!exists && !options.create) {
      throw vscode16.FileSystemError.FileNotFound(uri);
    }
    const ok = await this.fileService.writeFile(uri.path, content);
    if (!ok) {
      throw vscode16.FileSystemError.Unavailable(t("Write failed"));
    }
    this.fireChanged(uri, exists ? vscode16.FileChangeType.Changed : vscode16.FileChangeType.Created);
  }
  async rename(oldUri, newUri, options) {
    this.assertAvailable();
    this.assertWritablePath(oldUri);
    this.assertWritablePath(newUri);
    if (!await this.exists(oldUri)) {
      throw vscode16.FileSystemError.FileNotFound(oldUri);
    }
    const targetExists = await this.exists(newUri);
    if (targetExists) {
      if (!options.overwrite) {
        throw vscode16.FileSystemError.FileExists(newUri);
      }
      await this.delete(newUri, { recursive: true });
    }
    const ok = await this.fileService.renameFile(oldUri.path, newUri.path);
    if (!ok) {
      throw vscode16.FileSystemError.Unavailable(t("Rename failed"));
    }
    this._emitter.fire([
      { type: vscode16.FileChangeType.Deleted, uri: oldUri },
      { type: vscode16.FileChangeType.Created, uri: newUri },
      { type: vscode16.FileChangeType.Changed, uri: this.parentUri(oldUri) },
      { type: vscode16.FileChangeType.Changed, uri: this.parentUri(newUri) }
    ]);
  }
  async delete(uri, options) {
    this.assertAvailable();
    this.assertWritablePath(uri);
    const stat = await this.stat(uri);
    if (stat.type === vscode16.FileType.Directory && !options.recursive) {
      const entries = await this.readDirectory(uri);
      if (entries.length > 0) {
        throw vscode16.FileSystemError.NoPermissions(t("Directory is not empty"));
      }
    }
    const ok = stat.type === vscode16.FileType.Directory ? await this.fileService.rmdir(uri.path, options.recursive) : await this.fileService.deleteFile(uri.path);
    if (!ok) {
      throw vscode16.FileSystemError.Unavailable(t("Delete failed"));
    }
    this.fireChanged(uri, vscode16.FileChangeType.Deleted);
  }
  async createDirectory(uri) {
    this.assertAvailable();
    this.assertWritablePath(uri);
    const ok = await this.fileService.mkdir(uri.path);
    if (!ok) {
      throw vscode16.FileSystemError.Unavailable(t("Create directory failed"));
    }
    this.fireChanged(uri, vscode16.FileChangeType.Created);
  }
  watch(_uri, _options) {
    return new vscode16.Disposable(() => {
    });
  }
  assertWritablePath(uri) {
    if (!this.isWritablePath(uri.path)) {
      throw vscode16.FileSystemError.NoPermissions(t("CanMV root folders are read-only"));
    }
  }
  assertAvailable() {
    if (!this.availability.isAvailable()) {
      throw vscode16.FileSystemError.Unavailable(this.availability.unavailableMessage());
    }
  }
  isWritablePath(path15) {
    const parts = path15.split("/").filter(Boolean);
    return parts.length > 1 && WRITABLE_ROOTS.has(parts[0]);
  }
  async listDir(path15) {
    this.assertAvailable();
    try {
      return await this.fileService.listDir(path15);
    } catch {
      throw vscode16.FileSystemError.FileNotFound(vscode16.Uri.from({ scheme: "canmv", path: path15 }));
    }
  }
  async findEntry(uri) {
    const parent = this.parentPath(uri.path);
    const name = this.basename(uri.path);
    try {
      const entries = await this.listDir(parent);
      return entries.find((e) => e.name === name);
    } catch {
      return void 0;
    }
  }
  async exists(uri) {
    try {
      await this.stat(uri);
      return true;
    } catch {
      return false;
    }
  }
  fireChanged(uri, type) {
    this._emitter.fire([
      { type, uri },
      { type: vscode16.FileChangeType.Changed, uri: this.parentUri(uri) }
    ]);
  }
  parentUri(uri) {
    return uri.with({ path: this.parentPath(uri.path) });
  }
  parentPath(path15) {
    if (!path15 || path15 === "/") return "/";
    const trimmed = path15.replace(/\/+$/g, "");
    const index = trimmed.lastIndexOf("/");
    return index <= 0 ? "/" : trimmed.slice(0, index);
  }
  basename(path15) {
    return path15.replace(/\/+$/g, "").split("/").pop() || "";
  }
};

// src/webview/ToolHost.ts
var ToolRegistry = class {
  constructor() {
    this.tools = /* @__PURE__ */ new Map();
  }
  register(desc) {
    this.tools.set(desc.id, desc);
  }
  get(id) {
    return this.tools.get(id);
  }
  list() {
    return [...this.tools.values()];
  }
  listVisible() {
    return this.list().filter((t2) => t2.visible !== false);
  }
};
var ToolHost = class {
  constructor(registry) {
    this.registry = registry;
    this.panels = /* @__PURE__ */ new Map();
  }
  open(id) {
    const existing = this.panels.get(id);
    if (existing && !existing.disposed) {
      existing.reveal();
      return existing;
    }
    if (existing) {
      this.panels.delete(id);
    }
    const desc = this.registry.get(id);
    if (!desc) {
      throw new Error(t("Tool not found: {id}", { id }));
    }
    const panel = desc.factory();
    this.panels.set(id, panel);
    return panel;
  }
  close(id) {
    const panel = this.panels.get(id);
    if (panel) {
      panel.dispose();
      this.panels.delete(id);
    }
  }
  closeAll() {
    for (const [id, panel] of this.panels) {
      panel.dispose();
    }
    this.panels.clear();
  }
};

// src/webview/CanmvControlViewProvider.ts
var vscode17 = __toESM(require("vscode"));
var CanmvControlViewProvider = class {
  constructor(context) {
    this.context = context;
    this.state = {
      connected: false,
      scriptRunning: false,
      boardReady: false,
      connectionPhase: "idle",
      statusText: states.disconnected()
    };
  }
  resolveWebviewView(webviewView) {
    this.view = webviewView;
    webviewView.webview.options = { enableScripts: true };
    webviewView.webview.html = this.html();
    webviewView.webview.onDidReceiveMessage((msg) => {
      void this.handleMessage(msg);
    });
    webviewView.onDidDispose(() => {
      if (this.view === webviewView) {
        this.view = void 0;
      }
    });
  }
  setState(patch) {
    this.state = { ...this.state, ...patch };
    this.post({ type: "state", state: this.state });
  }
  async handleMessage(msg) {
    if (!msg || typeof msg.type !== "string") return;
    if (msg.type === "ready") {
      this.post({ type: "state", state: this.state });
      return;
    }
    if (msg.type === "command" && typeof msg.command === "string") {
      await vscode17.commands.executeCommand(msg.command);
      return;
    }
  }
  post(message) {
    if (this.view) {
      void this.view.webview.postMessage(message);
    }
  }
  html() {
    const nonce = getNonce();
    const csp = [
      "default-src 'none'",
      `script-src 'nonce-${nonce}'`,
      "style-src 'unsafe-inline'"
    ].join("; ");
    return `<!doctype html>
<html lang="${escapeHtml(vscode17.env.language || "en")}">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <meta http-equiv="Content-Security-Policy" content="${csp}">
  <style>
    * { box-sizing: border-box; }
    html, body {
      margin: 0;
      min-height: 0;
      color: var(--vscode-foreground);
      background: var(--vscode-sideBar-background);
      font: var(--vscode-font-size) var(--vscode-font-family);
      overflow: hidden;
    }
    .bar {
      min-height: 28px;
      padding: 2px 6px;
    }
    .status {
      display: grid;
      grid-template-columns: 8px minmax(0, 1fr) auto;
      align-items: center;
      gap: 6px;
      min-width: 0;
    }
    .dot {
      width: 8px;
      height: 8px;
      border-radius: 50%;
      background: var(--vscode-testing-iconFailed);
    }
    .dot.connected {
      background: var(--vscode-testing-iconPassed);
    }
    .status-main {
      min-width: 0;
      overflow: hidden;
    }
    .status-title {
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
      color: var(--vscode-foreground);
      font-weight: 500;
    }
    .status-subtitle {
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
      color: var(--vscode-descriptionForeground);
      font-size: 11px;
      margin-top: 1px;
    }
    .badge {
      min-width: 42px;
      color: var(--vscode-descriptionForeground);
      font-size: 11px;
      text-align: right;
    }
    .badge.idle {
      color: var(--vscode-descriptionForeground);
      background: transparent;
    }
  </style>
</head>
<body>
  <div class="bar">
    <div class="status">
      <span id="status-dot" class="dot"></span>
      <span class="status-main">
        <span id="status-title" class="status-title">CanMV</span>
        <span id="status-text" class="status-subtitle">${escapeHtml(states.disconnected())}</span>
      </span>
      <span id="status-badge" class="badge idle">${escapeHtml(states.offline())}</span>
    </div>
  </div>

  <script nonce="${nonce}">
    const vscode = acquireVsCodeApi();
    const statusDot = document.getElementById('status-dot');
    const statusTitle = document.getElementById('status-title');
    const statusText = document.getElementById('status-text');
    const statusBadge = document.getElementById('status-badge');
    const l10n = ${jsonForScript2({
      canmv: "CanMV",
      canmvBoard: states.canmvBoard(),
      connecting: states.connecting(),
      disconnecting: states.disconnecting(),
      preparing: states.preparing(),
      connected: states.connected(),
      disconnected: states.disconnected(),
      offline: states.offline(),
      ready: states.ready(),
      running: states.running()
    })};
    let state = { connected: false, scriptRunning: false, boardReady: false, connectionPhase: 'idle', statusText: l10n.disconnected };

    window.addEventListener('message', event => {
      const msg = event.data || {};
      if (msg.type === 'state') {
        state = msg.state || state;
        renderState();
      }
    });

    function renderState() {
      statusTitle.textContent = state.connected ? l10n.canmvBoard : l10n.canmv;
      statusText.textContent = state.statusText || (state.connected ? l10n.connected : l10n.disconnected);
      statusBadge.textContent = badgeText();
      statusBadge.classList.toggle('idle', !state.connected || !state.scriptRunning);
      statusDot.classList.toggle('connected', !!state.connected);
    }

    function badgeText() {
      if (state.connectionPhase === 'disconnecting') return l10n.disconnecting;
      if (state.connectionPhase === 'connecting') return l10n.connecting;
      if (state.scriptRunning) return l10n.running;
      if (state.connected) return state.boardReady ? l10n.ready : l10n.preparing;
      return l10n.offline;
    }

    renderState();
    vscode.postMessage({ type: 'ready' });
  </script>
</body>
</html>`;
  }
};
function getNonce() {
  const chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  let value = "";
  for (let i = 0; i < 32; i++) {
    value += chars.charAt(Math.floor(Math.random() * chars.length));
  }
  return value;
}
function escapeHtml(value) {
  return value.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;");
}
function jsonForScript2(value) {
  return JSON.stringify(value).replace(/</g, "\\u003c").replace(/>/g, "\\u003e").replace(/&/g, "\\u0026").replace(/\u2028/g, "\\u2028").replace(/\u2029/g, "\\u2029");
}

// src/webview/ToolboxTreeProvider.ts
var vscode18 = __toESM(require("vscode"));
var ToolboxTreeProvider = class {
  constructor(registry) {
    this.registry = registry;
    this._onDidChangeTreeData = new vscode18.EventEmitter();
    this.onDidChangeTreeData = this._onDidChangeTreeData.event;
  }
  refresh() {
    this._onDidChangeTreeData.fire();
  }
  getChildren() {
    return this.registry.listVisible().map((tool) => {
      const item = new vscode18.TreeItem(tool.name, vscode18.TreeItemCollapsibleState.None);
      item.iconPath = new vscode18.ThemeIcon(tool.icon);
      item.command = { command: "canmv.openTool", title: tool.name, arguments: [tool.id] };
      return item;
    });
  }
  getTreeItem(element) {
    return element;
  }
};

// src/service/examplesService.ts
var vscode19 = __toESM(require("vscode"));
var fs8 = __toESM(require("fs"));
var path9 = __toESM(require("path"));
var os4 = __toESM(require("os"));
var import_child_process2 = require("child_process");
var ExamplesService = class _ExamplesService {
  constructor(context, routeService) {
    this.context = context;
    this.routeService = routeService;
    this._onDidChangeExamples = new vscode19.EventEmitter();
    this.onDidChangeExamples = this._onDidChangeExamples.event;
    this.baseDir = path9.join(os4.homedir(), ".kendryte", "k230_canmv_examples");
  }
  static {
    this.lastExamplesKey = "canmv.examples.lastId";
  }
  async ensureExamples(route) {
    const examplesId = normalizeExamplesId(route?.examplesId || "");
    if (!examplesId) return this.ensureLocalExamples();
    if (this.isCacheUsable(examplesId)) {
      const cacheDir2 = this.cacheDirFor(examplesId);
      logInfo("Examples", `Using local examples: ${examplesId} (${cacheDir2})`);
      await this.context.globalState.update(_ExamplesService.lastExamplesKey, examplesId);
      this._onDidChangeExamples.fire();
      return cacheDir2;
    }
    if (!this.canAutoDownload()) {
      logInfo("Examples", `Examples are not cached and auto-download is disabled: ${examplesId}`);
      return null;
    }
    if (!route?.examplesUrl) {
      logWarn("Examples", `Firmware manifest does not include examples URL: ${examplesId}`);
      return null;
    }
    const cacheDir = this.cacheDirFor(examplesId);
    if (await this.downloadAndExtract(examplesId, route.examplesUrl, cacheDir)) {
      await this.context.globalState.update(_ExamplesService.lastExamplesKey, examplesId);
      this._onDidChangeExamples.fire();
      return cacheDir;
    }
    return null;
  }
  activeExamplesDir() {
    const lastExamplesId = this.context.globalState.get(_ExamplesService.lastExamplesKey) || "";
    const normalized = normalizeExamplesId(lastExamplesId);
    if (normalized && this.isCacheUsable(normalized)) {
      return this.cacheDirFor(normalized);
    }
    const localExamplesId = this.findLatestLocalExamplesId();
    return localExamplesId ? this.cacheDirFor(localExamplesId) : "";
  }
  examplesRootDir() {
    return this.baseDir;
  }
  refresh() {
    this._onDidChangeExamples.fire();
  }
  canAutoDownload() {
    const autoDownload = vscode19.workspace.getConfiguration("canmv").get("stubsAutoDownload", true);
    if (!autoDownload) {
      logInfo("Examples", "Auto-download disabled (canmv.stubsAutoDownload = false)");
      return false;
    }
    return true;
  }
  cacheDirFor(examplesId) {
    return path9.join(this.baseDir, examplesId);
  }
  isCacheUsable(examplesId) {
    const normalized = normalizeExamplesId(examplesId);
    if (!normalized) return false;
    const cacheDir = this.cacheDirFor(normalized);
    try {
      return fs8.existsSync(cacheDir) && this.hasExtractedContent(cacheDir);
    } catch {
      return false;
    }
  }
  async ensureLocalExamples() {
    const lastExamplesId = this.context.globalState.get(_ExamplesService.lastExamplesKey) || "";
    const normalized = normalizeExamplesId(lastExamplesId);
    if (normalized && this.isCacheUsable(normalized)) {
      const cacheDir = this.cacheDirFor(normalized);
      logInfo("Examples", `Using last configured local examples: ${normalized} (${cacheDir})`);
      return cacheDir;
    }
    const localExamplesId = this.findLatestLocalExamplesId();
    if (localExamplesId) {
      const cacheDir = this.cacheDirFor(localExamplesId);
      logInfo("Examples", `Using latest local cached examples: ${localExamplesId} (${cacheDir})`);
      await this.context.globalState.update(_ExamplesService.lastExamplesKey, localExamplesId);
      this._onDidChangeExamples.fire();
      return cacheDir;
    }
    return null;
  }
  findLatestLocalExamplesId() {
    try {
      if (!fs8.existsSync(this.baseDir)) return "";
      const examples = fs8.readdirSync(this.baseDir, { withFileTypes: true }).filter((entry) => entry.isDirectory() && this.isCacheUsable(entry.name)).map((entry) => {
        const examplesId = entry.name;
        const mtime = fs8.statSync(this.cacheDirFor(examplesId)).mtimeMs;
        return { examplesId, mtime };
      }).sort((a, b) => b.mtime - a.mtime);
      return examples[0]?.examplesId || "";
    } catch {
      return "";
    }
  }
  hasExtractedContent(cacheDir) {
    try {
      const entries = fs8.readdirSync(cacheDir, { withFileTypes: true });
      return entries.some(
        (entry) => entry.isDirectory() && (entry.name === "examples" || entry.name === "models")
      );
    } catch {
      return false;
    }
  }
  async downloadAndExtract(examplesId, zipUrl, cacheDir) {
    logInfo("Examples", `Downloading examples archive: ${zipUrl}`);
    try {
      const data = await this.routeService.fetchBuffer(zipUrl);
      if (!data || data.length === 0) {
        logWarn("Examples", `Empty response from examples archive: ${examplesId}`);
        return false;
      }
      fs8.mkdirSync(cacheDir, { recursive: true });
      const zipPath = path9.join(cacheDir, "examples.zip");
      fs8.writeFileSync(zipPath, data);
      await this.extractArchive(zipPath, cacheDir);
      fs8.unlinkSync(zipPath);
      if (this.isCacheUsable(examplesId)) {
        logInfo("Examples", `Extracted examples archive: ${data.length} bytes -> ${cacheDir}`);
        return true;
      }
      this.cleanupFailedDownload(cacheDir);
      logWarn("Examples", `Extracted archive did not contain example files: ${examplesId}`);
      return false;
    } catch (err) {
      logError("Examples", `Download/extract failed for ${examplesId}: ${err}`);
      this.cleanupFailedDownload(cacheDir);
      return false;
    }
  }
  async extractArchive(archivePath, targetDir) {
    const backend2 = resolveNativeBackendCommand(this.context);
    await new Promise((resolve3, reject) => {
      (0, import_child_process2.execFile)(
        backend2.command,
        [...backend2.args, "--extract-archive", archivePath, targetDir],
        { cwd: backend2.cwd, windowsHide: true, timeout: 6e4 },
        (err, stdout, stderr) => {
          if (err) {
            const detail = stderr?.trim() || stdout?.trim() || err.message;
            reject(new Error(detail));
            return;
          }
          resolve3();
        }
      );
    });
  }
  cleanupFailedDownload(targetDir) {
    try {
      if (!fs8.existsSync(targetDir) || this.hasExtractedContent(targetDir)) {
        return;
      }
      for (const entry of fs8.readdirSync(targetDir)) {
        fs8.rmSync(path9.join(targetDir, entry), { recursive: true, force: true });
      }
      fs8.rmdirSync(targetDir);
    } catch {
    }
  }
};

// src/webview/ThresholdEditorPanel.ts
var vscode20 = __toESM(require("vscode"));
var ThresholdEditorPanel = class extends BaseToolPanel {
  constructor(context) {
    super("canmvThresholdEditor", t("Threshold Editor"), context, "threshold.html");
    this._onCopyThreshold = new vscode20.EventEmitter();
    this._onApplyThreshold = new vscode20.EventEmitter();
    this._onRequestPreviewFrame = new vscode20.EventEmitter();
    this.config = {};
    this.ready = false;
    this.onCopyThreshold = this._onCopyThreshold.event;
    this.onApplyThreshold = this._onApplyThreshold.event;
    this.onRequestPreviewFrame = this._onRequestPreviewFrame.event;
    this.panel.webview.onDidReceiveMessage((msg) => {
      if (!msg || typeof msg.type !== "string") return;
      if (msg.type === "ready") {
        this.ready = true;
        this.sendConfig();
      } else if (msg.type === "copyThreshold" && typeof msg.text === "string") {
        this._onCopyThreshold.fire(msg.text);
      } else if (msg.type === "applyThreshold" && typeof msg.text === "string") {
        this._onApplyThreshold.fire(msg.text);
      } else if (msg.type === "requestPreviewFrame") {
        this._onRequestPreviewFrame.fire();
      }
    });
  }
  configure(config) {
    this.config = { ...config };
    this.sendConfig();
  }
  sendPreviewFrame(data, name = t("Preview Frame")) {
    this.postMessage({
      type: "previewFrame",
      data,
      byteLength: data.byteLength,
      name
    });
  }
  sendFrameUnavailable(message) {
    this.postMessage({ type: "frameUnavailable", message });
  }
  sendCopied() {
    this.postMessage({ type: "copied" });
  }
  sendApplied() {
    this.postMessage({ type: "applied" });
  }
  sendConfig() {
    if (!this.ready) return;
    this.postMessage({ type: "configure", config: this.config });
  }
};

// src/mcp/provider.ts
var vscode23 = __toESM(require("vscode"));

// src/mcp/clientRegistration.ts
var cp2 = __toESM(require("child_process"));
var crypto3 = __toESM(require("crypto"));
var fs9 = __toESM(require("fs"));
var os5 = __toESM(require("os"));
var path10 = __toESM(require("path"));
var vscode21 = __toESM(require("vscode"));

// node_modules/smol-toml/dist/error.js
function getLineColFromPtr(string, ptr) {
  let lines = string.slice(0, ptr).split(/\r\n|\n|\r/g);
  return [lines.length, lines.pop().length + 1];
}
function makeCodeBlock(string, line, column) {
  let lines = string.split(/\r\n|\n|\r/g);
  let codeblock = "";
  let numberLen = (Math.log10(line + 1) | 0) + 1;
  for (let i = line - 1; i <= line + 1; i++) {
    let l = lines[i - 1];
    if (!l)
      continue;
    codeblock += i.toString().padEnd(numberLen, " ");
    codeblock += ":  ";
    codeblock += l;
    codeblock += "\n";
    if (i === line) {
      codeblock += " ".repeat(numberLen + column + 2);
      codeblock += "^\n";
    }
  }
  return codeblock;
}
var TomlError = class extends Error {
  line;
  column;
  codeblock;
  constructor(message, options) {
    const [line, column] = getLineColFromPtr(options.toml, options.ptr);
    const codeblock = makeCodeBlock(options.toml, line, column);
    super(`Invalid TOML document: ${message}

${codeblock}`, options);
    this.line = line;
    this.column = column;
    this.codeblock = codeblock;
  }
};

// node_modules/smol-toml/dist/util.js
function isEscaped(str, ptr) {
  let i = 0;
  while (str[ptr - ++i] === "\\")
    ;
  return --i && i % 2;
}
function indexOfNewline(str, start = 0, end = str.length) {
  let idx = str.indexOf("\n", start);
  if (str[idx - 1] === "\r")
    idx--;
  return idx <= end ? idx : -1;
}
function skipComment(str, ptr) {
  for (let i = ptr; i < str.length; i++) {
    let c = str[i];
    if (c === "\n")
      return i;
    if (c === "\r" && str[i + 1] === "\n")
      return i + 1;
    if (c < " " && c !== "	" || c === "\x7F") {
      throw new TomlError("control characters are not allowed in comments", {
        toml: str,
        ptr
      });
    }
  }
  return str.length;
}
function skipVoid(str, ptr, banNewLines, banComments) {
  let c;
  while (1) {
    while ((c = str[ptr]) === " " || c === "	" || !banNewLines && (c === "\n" || c === "\r" && str[ptr + 1] === "\n"))
      ptr++;
    if (banComments || c !== "#")
      break;
    ptr = skipComment(str, ptr);
  }
  return ptr;
}
function skipUntil(str, ptr, sep2, end, banNewLines = false) {
  if (!end) {
    ptr = indexOfNewline(str, ptr);
    return ptr < 0 ? str.length : ptr;
  }
  for (let i = ptr; i < str.length; i++) {
    let c = str[i];
    if (c === "#") {
      i = indexOfNewline(str, i);
    } else if (c === sep2) {
      return i + 1;
    } else if (c === end || banNewLines && (c === "\n" || c === "\r" && str[i + 1] === "\n")) {
      return i;
    }
  }
  throw new TomlError("cannot find end of structure", {
    toml: str,
    ptr
  });
}
function getStringEnd(str, seek) {
  let first = str[seek];
  let target = first === str[seek + 1] && str[seek + 1] === str[seek + 2] ? str.slice(seek, seek + 3) : first;
  seek += target.length - 1;
  do
    seek = str.indexOf(target, ++seek);
  while (seek > -1 && first !== "'" && isEscaped(str, seek));
  if (seek > -1) {
    seek += target.length;
    if (target.length > 1) {
      if (str[seek] === first)
        seek++;
      if (str[seek] === first)
        seek++;
    }
  }
  return seek;
}

// node_modules/smol-toml/dist/date.js
var DATE_TIME_RE = /^(\d{4}-\d{2}-\d{2})?[T ]?(?:(\d{2}):\d{2}(?::\d{2}(?:\.\d+)?)?)?(Z|[-+]\d{2}:\d{2})?$/i;
var TomlDate = class _TomlDate extends Date {
  #hasDate = false;
  #hasTime = false;
  #offset = null;
  constructor(date) {
    let hasDate = true;
    let hasTime = true;
    let offset = "Z";
    if (typeof date === "string") {
      let match = date.match(DATE_TIME_RE);
      if (match) {
        if (!match[1]) {
          hasDate = false;
          date = `0000-01-01T${date}`;
        }
        hasTime = !!match[2];
        hasTime && date[10] === " " && (date = date.replace(" ", "T"));
        if (match[2] && +match[2] > 23) {
          date = "";
        } else {
          offset = match[3] || null;
          date = date.toUpperCase();
          if (!offset && hasTime)
            date += "Z";
        }
      } else {
        date = "";
      }
    }
    super(date);
    if (!isNaN(this.getTime())) {
      this.#hasDate = hasDate;
      this.#hasTime = hasTime;
      this.#offset = offset;
    }
  }
  isDateTime() {
    return this.#hasDate && this.#hasTime;
  }
  isLocal() {
    return !this.#hasDate || !this.#hasTime || !this.#offset;
  }
  isDate() {
    return this.#hasDate && !this.#hasTime;
  }
  isTime() {
    return this.#hasTime && !this.#hasDate;
  }
  isValid() {
    return this.#hasDate || this.#hasTime;
  }
  toISOString() {
    let iso = super.toISOString();
    if (this.isDate())
      return iso.slice(0, 10);
    if (this.isTime())
      return iso.slice(11, 23);
    if (this.#offset === null)
      return iso.slice(0, -1);
    if (this.#offset === "Z")
      return iso;
    let offset = +this.#offset.slice(1, 3) * 60 + +this.#offset.slice(4, 6);
    offset = this.#offset[0] === "-" ? offset : -offset;
    let offsetDate = new Date(this.getTime() - offset * 6e4);
    return offsetDate.toISOString().slice(0, -1) + this.#offset;
  }
  static wrapAsOffsetDateTime(jsDate, offset = "Z") {
    let date = new _TomlDate(jsDate);
    date.#offset = offset;
    return date;
  }
  static wrapAsLocalDateTime(jsDate) {
    let date = new _TomlDate(jsDate);
    date.#offset = null;
    return date;
  }
  static wrapAsLocalDate(jsDate) {
    let date = new _TomlDate(jsDate);
    date.#hasTime = false;
    date.#offset = null;
    return date;
  }
  static wrapAsLocalTime(jsDate) {
    let date = new _TomlDate(jsDate);
    date.#hasDate = false;
    date.#offset = null;
    return date;
  }
};

// node_modules/smol-toml/dist/primitive.js
var INT_REGEX = /^((0x[0-9a-fA-F](_?[0-9a-fA-F])*)|(([+-]|0[ob])?\d(_?\d)*))$/;
var FLOAT_REGEX = /^[+-]?\d(_?\d)*(\.\d(_?\d)*)?([eE][+-]?\d(_?\d)*)?$/;
var LEADING_ZERO = /^[+-]?0[0-9_]/;
var ESCAPE_REGEX = /^[0-9a-f]{2,8}$/i;
var ESC_MAP = {
  b: "\b",
  t: "	",
  n: "\n",
  f: "\f",
  r: "\r",
  e: "\x1B",
  '"': '"',
  "\\": "\\"
};
function parseString(str, ptr = 0, endPtr = str.length) {
  let isLiteral = str[ptr] === "'";
  let isMultiline = str[ptr++] === str[ptr] && str[ptr] === str[ptr + 1];
  if (isMultiline) {
    endPtr -= 2;
    if (str[ptr += 2] === "\r")
      ptr++;
    if (str[ptr] === "\n")
      ptr++;
  }
  let tmp = 0;
  let isEscape;
  let parsed = "";
  let sliceStart = ptr;
  while (ptr < endPtr - 1) {
    let c = str[ptr++];
    if (c === "\n" || c === "\r" && str[ptr] === "\n") {
      if (!isMultiline) {
        throw new TomlError("newlines are not allowed in strings", {
          toml: str,
          ptr: ptr - 1
        });
      }
    } else if (c < " " && c !== "	" || c === "\x7F") {
      throw new TomlError("control characters are not allowed in strings", {
        toml: str,
        ptr: ptr - 1
      });
    }
    if (isEscape) {
      isEscape = false;
      if (c === "x" || c === "u" || c === "U") {
        let code = str.slice(ptr, ptr += c === "x" ? 2 : c === "u" ? 4 : 8);
        if (!ESCAPE_REGEX.test(code)) {
          throw new TomlError("invalid unicode escape", {
            toml: str,
            ptr: tmp
          });
        }
        try {
          parsed += String.fromCodePoint(parseInt(code, 16));
        } catch {
          throw new TomlError("invalid unicode escape", {
            toml: str,
            ptr: tmp
          });
        }
      } else if (isMultiline && (c === "\n" || c === " " || c === "	" || c === "\r")) {
        ptr = skipVoid(str, ptr - 1, true);
        if (str[ptr] !== "\n" && str[ptr] !== "\r") {
          throw new TomlError("invalid escape: only line-ending whitespace may be escaped", {
            toml: str,
            ptr: tmp
          });
        }
        ptr = skipVoid(str, ptr);
      } else if (c in ESC_MAP) {
        parsed += ESC_MAP[c];
      } else {
        throw new TomlError("unrecognized escape sequence", {
          toml: str,
          ptr: tmp
        });
      }
      sliceStart = ptr;
    } else if (!isLiteral && c === "\\") {
      tmp = ptr - 1;
      isEscape = true;
      parsed += str.slice(sliceStart, tmp);
    }
  }
  return parsed + str.slice(sliceStart, endPtr - 1);
}
function parseValue(value, toml, ptr, integersAsBigInt) {
  if (value === "true")
    return true;
  if (value === "false")
    return false;
  if (value === "-inf")
    return -Infinity;
  if (value === "inf" || value === "+inf")
    return Infinity;
  if (value === "nan" || value === "+nan" || value === "-nan")
    return NaN;
  if (value === "-0")
    return integersAsBigInt ? 0n : 0;
  let isInt = INT_REGEX.test(value);
  if (isInt || FLOAT_REGEX.test(value)) {
    if (LEADING_ZERO.test(value)) {
      throw new TomlError("leading zeroes are not allowed", {
        toml,
        ptr
      });
    }
    value = value.replace(/_/g, "");
    let numeric = +value;
    if (isNaN(numeric)) {
      throw new TomlError("invalid number", {
        toml,
        ptr
      });
    }
    if (isInt) {
      if ((isInt = !Number.isSafeInteger(numeric)) && !integersAsBigInt) {
        throw new TomlError("integer value cannot be represented losslessly", {
          toml,
          ptr
        });
      }
      if (isInt || integersAsBigInt === true)
        numeric = BigInt(value);
    }
    return numeric;
  }
  const date = new TomlDate(value);
  if (!date.isValid()) {
    throw new TomlError("invalid value", {
      toml,
      ptr
    });
  }
  return date;
}

// node_modules/smol-toml/dist/extract.js
function sliceAndTrimEndOf(str, startPtr, endPtr) {
  let value = str.slice(startPtr, endPtr);
  let commentIdx = value.indexOf("#");
  if (commentIdx > -1) {
    skipComment(str, commentIdx);
    value = value.slice(0, commentIdx);
  }
  return [value.trimEnd(), commentIdx];
}
function extractValue(str, ptr, end, depth, integersAsBigInt) {
  if (depth === 0) {
    throw new TomlError("document contains excessively nested structures. aborting.", {
      toml: str,
      ptr
    });
  }
  let c = str[ptr];
  if (c === "[" || c === "{") {
    let [value, endPtr2] = c === "[" ? parseArray(str, ptr, depth, integersAsBigInt) : parseInlineTable(str, ptr, depth, integersAsBigInt);
    if (end) {
      endPtr2 = skipVoid(str, endPtr2);
      if (str[endPtr2] === ",")
        endPtr2++;
      else if (str[endPtr2] !== end) {
        throw new TomlError("expected comma or end of structure", {
          toml: str,
          ptr: endPtr2
        });
      }
    }
    return [value, endPtr2];
  }
  let endPtr;
  if (c === '"' || c === "'") {
    endPtr = getStringEnd(str, ptr);
    let parsed = parseString(str, ptr, endPtr);
    if (end) {
      endPtr = skipVoid(str, endPtr);
      if (str[endPtr] && str[endPtr] !== "," && str[endPtr] !== end && str[endPtr] !== "\n" && str[endPtr] !== "\r") {
        throw new TomlError("unexpected character encountered", {
          toml: str,
          ptr: endPtr
        });
      }
      endPtr += +(str[endPtr] === ",");
    }
    return [parsed, endPtr];
  }
  endPtr = skipUntil(str, ptr, ",", end);
  let slice = sliceAndTrimEndOf(str, ptr, endPtr - +(str[endPtr - 1] === ","));
  if (!slice[0]) {
    throw new TomlError("incomplete key-value declaration: no value specified", {
      toml: str,
      ptr
    });
  }
  if (end && slice[1] > -1) {
    endPtr = skipVoid(str, ptr + slice[1]);
    endPtr += +(str[endPtr] === ",");
  }
  return [
    parseValue(slice[0], str, ptr, integersAsBigInt),
    endPtr
  ];
}

// node_modules/smol-toml/dist/struct.js
var KEY_PART_RE = /^[a-zA-Z0-9-_]+[ \t]*$/;
function parseKey(str, ptr, end = "=") {
  let dot = ptr - 1;
  let parsed = [];
  let endPtr = str.indexOf(end, ptr);
  if (endPtr < 0) {
    throw new TomlError("incomplete key-value: cannot find end of key", {
      toml: str,
      ptr
    });
  }
  do {
    let c = str[ptr = ++dot];
    if (c !== " " && c !== "	") {
      if (c === '"' || c === "'") {
        if (c === str[ptr + 1] && c === str[ptr + 2]) {
          throw new TomlError("multiline strings are not allowed in keys", {
            toml: str,
            ptr
          });
        }
        let eos = getStringEnd(str, ptr);
        if (eos < 0) {
          throw new TomlError("unfinished string encountered", {
            toml: str,
            ptr
          });
        }
        dot = str.indexOf(".", eos);
        let strEnd = str.slice(eos, dot < 0 || dot > endPtr ? endPtr : dot);
        let newLine = indexOfNewline(strEnd);
        if (newLine > -1) {
          throw new TomlError("newlines are not allowed in keys", {
            toml: str,
            ptr: ptr + dot + newLine
          });
        }
        if (strEnd.trimStart()) {
          throw new TomlError("found extra tokens after the string part", {
            toml: str,
            ptr: eos
          });
        }
        if (endPtr < eos) {
          endPtr = str.indexOf(end, eos);
          if (endPtr < 0) {
            throw new TomlError("incomplete key-value: cannot find end of key", {
              toml: str,
              ptr
            });
          }
        }
        parsed.push(parseString(str, ptr, eos));
      } else {
        dot = str.indexOf(".", ptr);
        let part = str.slice(ptr, dot < 0 || dot > endPtr ? endPtr : dot);
        if (!KEY_PART_RE.test(part)) {
          throw new TomlError("only letter, numbers, dashes and underscores are allowed in keys", {
            toml: str,
            ptr
          });
        }
        parsed.push(part.trimEnd());
      }
    }
  } while (dot + 1 && dot < endPtr);
  return [parsed, skipVoid(str, endPtr + 1, true, true)];
}
function parseInlineTable(str, ptr, depth, integersAsBigInt) {
  let res = {};
  let seen = /* @__PURE__ */ new Set();
  let c;
  ptr++;
  while ((c = str[ptr++]) !== "}" && c) {
    if (c === ",") {
      throw new TomlError("expected value, found comma", {
        toml: str,
        ptr: ptr - 1
      });
    } else if (c === "#")
      ptr = skipComment(str, ptr);
    else if (c !== " " && c !== "	" && c !== "\n" && c !== "\r") {
      let k;
      let t2 = res;
      let hasOwn = false;
      let [key, keyEndPtr] = parseKey(str, ptr - 1);
      for (let i = 0; i < key.length; i++) {
        if (i)
          t2 = hasOwn ? t2[k] : t2[k] = {};
        k = key[i];
        if ((hasOwn = Object.hasOwn(t2, k)) && (typeof t2[k] !== "object" || seen.has(t2[k]))) {
          throw new TomlError("trying to redefine an already defined value", {
            toml: str,
            ptr
          });
        }
        if (!hasOwn && k === "__proto__") {
          Object.defineProperty(t2, k, { enumerable: true, configurable: true, writable: true });
        }
      }
      if (hasOwn) {
        throw new TomlError("trying to redefine an already defined value", {
          toml: str,
          ptr
        });
      }
      let [value, valueEndPtr] = extractValue(str, keyEndPtr, "}", depth - 1, integersAsBigInt);
      seen.add(value);
      t2[k] = value;
      ptr = valueEndPtr;
    }
  }
  if (!c) {
    throw new TomlError("unfinished table encountered", {
      toml: str,
      ptr
    });
  }
  return [res, ptr];
}
function parseArray(str, ptr, depth, integersAsBigInt) {
  let res = [];
  let c;
  ptr++;
  while ((c = str[ptr++]) !== "]" && c) {
    if (c === ",") {
      throw new TomlError("expected value, found comma", {
        toml: str,
        ptr: ptr - 1
      });
    } else if (c === "#")
      ptr = skipComment(str, ptr);
    else if (c !== " " && c !== "	" && c !== "\n" && c !== "\r") {
      let e = extractValue(str, ptr - 1, "]", depth - 1, integersAsBigInt);
      res.push(e[0]);
      ptr = e[1];
    }
  }
  if (!c) {
    throw new TomlError("unfinished array encountered", {
      toml: str,
      ptr
    });
  }
  return [res, ptr];
}

// node_modules/smol-toml/dist/parse.js
function peekTable(key, table, meta, type) {
  let t2 = table;
  let m = meta;
  let k;
  let hasOwn = false;
  let state;
  for (let i = 0; i < key.length; i++) {
    if (i) {
      t2 = hasOwn ? t2[k] : t2[k] = {};
      m = (state = m[k]).c;
      if (type === 0 && (state.t === 1 || state.t === 2)) {
        return null;
      }
      if (state.t === 2) {
        let l = t2.length - 1;
        t2 = t2[l];
        m = m[l].c;
      }
    }
    k = key[i];
    if ((hasOwn = Object.hasOwn(t2, k)) && m[k]?.t === 0 && m[k]?.d) {
      return null;
    }
    if (!hasOwn) {
      if (k === "__proto__") {
        Object.defineProperty(t2, k, { enumerable: true, configurable: true, writable: true });
        Object.defineProperty(m, k, { enumerable: true, configurable: true, writable: true });
      }
      m[k] = {
        t: i < key.length - 1 && type === 2 ? 3 : type,
        d: false,
        i: 0,
        c: {}
      };
    }
  }
  state = m[k];
  if (state.t !== type && !(type === 1 && state.t === 3)) {
    return null;
  }
  if (type === 2) {
    if (!state.d) {
      state.d = true;
      t2[k] = [];
    }
    t2[k].push(t2 = {});
    state.c[state.i++] = state = { t: 1, d: false, i: 0, c: {} };
  }
  if (state.d) {
    return null;
  }
  state.d = true;
  if (type === 1) {
    t2 = hasOwn ? t2[k] : t2[k] = {};
  } else if (type === 0 && hasOwn) {
    return null;
  }
  return [k, t2, state.c];
}
function parse(toml, { maxDepth = 1e3, integersAsBigInt } = {}) {
  let res = {};
  let meta = {};
  let tbl = res;
  let m = meta;
  for (let ptr = skipVoid(toml, 0); ptr < toml.length; ) {
    if (toml[ptr] === "[") {
      let isTableArray = toml[++ptr] === "[";
      let k = parseKey(toml, ptr += +isTableArray, "]");
      if (isTableArray) {
        if (toml[k[1] - 1] !== "]") {
          throw new TomlError("expected end of table declaration", {
            toml,
            ptr: k[1] - 1
          });
        }
        k[1]++;
      }
      let p = peekTable(
        k[0],
        res,
        meta,
        isTableArray ? 2 : 1
        /* Type.EXPLICIT */
      );
      if (!p) {
        throw new TomlError("trying to redefine an already defined table or value", {
          toml,
          ptr
        });
      }
      m = p[2];
      tbl = p[1];
      ptr = k[1];
    } else {
      let k = parseKey(toml, ptr);
      let p = peekTable(
        k[0],
        tbl,
        m,
        0
        /* Type.DOTTED */
      );
      if (!p) {
        throw new TomlError("trying to redefine an already defined table or value", {
          toml,
          ptr
        });
      }
      let v = extractValue(toml, k[1], void 0, maxDepth, integersAsBigInt);
      p[1][p[0]] = v[0];
      ptr = v[1];
    }
    ptr = skipVoid(toml, ptr, true);
    if (toml[ptr] && toml[ptr] !== "\n" && toml[ptr] !== "\r") {
      throw new TomlError("each key-value declaration must be followed by an end-of-line", {
        toml,
        ptr
      });
    }
    ptr = skipVoid(toml, ptr);
  }
  return res;
}

// src/mcp/clientRegistration.ts
var MCP_SERVER_NAME = "canmv-k230";
var MANAGED_MARKER = "canmv-vscode";
var MANAGED_HEADER = "CANMV_MCP_MANAGED";
var AUTH_FINGERPRINT_HEADER_PREFIX = "CANMV_MCP_AUTH_";
var PROCESS_TIMEOUT_MS = 15e3;
var LEGACY_MANUAL_SETUP_FILES = [
  "README.txt",
  "codex-canmv-k230.toml",
  "claude-canmv-k230.json",
  "configure-canmv-mcp.ps1",
  "configure-canmv-mcp.sh"
];
var MCP_CLIENT_EXTENSION_IDS = {
  codex: "openai.chatgpt",
  claudeCode: "anthropic.claude-code"
};
function removeLegacyMcpManualSetup(context) {
  const directory = path10.join(context.globalStorageUri.fsPath, "mcp-manual-setup");
  try {
    const stat = fs9.lstatSync(directory);
    if (!stat.isDirectory() || stat.isSymbolicLink()) {
      logWarn("MCP", `Legacy manual setup path is not a normal directory; leaving it unchanged: ${directory}`);
      return;
    }
  } catch (err) {
    if (err.code !== "ENOENT") {
      logWarn("MCP", `Unable to inspect legacy manual setup directory: ${errorMessage(err)}`);
    }
    return;
  }
  let removed = 0;
  for (const filename of LEGACY_MANUAL_SETUP_FILES) {
    try {
      fs9.unlinkSync(path10.join(directory, filename));
      removed += 1;
    } catch (err) {
      if (err.code !== "ENOENT") {
        logWarn("MCP", `Unable to remove legacy manual setup file ${filename}: ${errorMessage(err)}`);
      }
    }
  }
  try {
    fs9.rmdirSync(directory);
  } catch (err) {
    const code = err.code;
    if (code !== "ENOENT" && code !== "ENOTEMPTY") {
      logWarn("MCP", `Unable to remove legacy manual setup directory: ${errorMessage(err)}`);
    }
  }
  if (removed > 0) logInfo("MCP", `Removed ${removed} legacy manual setup file(s)`);
}
async function configureExternalMcpClients(connection, wslRelay) {
  const result = {
    configured: [],
    unchanged: [],
    skipped: [],
    failed: [],
    missingExtensions: []
  };
  const codexExtensionPath = findClientExtensionPath(MCP_CLIENT_EXTENSION_IDS.codex);
  const claudeExtensionPath = findClientExtensionPath(MCP_CLIENT_EXTENSION_IDS.claudeCode);
  if (!codexExtensionPath) result.missingExtensions.push(MCP_CLIENT_EXTENSION_IDS.codex);
  if (!claudeExtensionPath) result.missingExtensions.push(MCP_CLIENT_EXTENSION_IDS.claudeCode);
  const codexRequiresWsl = shouldPreferWslCodex();
  let codexTarget;
  let codexResolutionFailed = false;
  if (codexRequiresWsl) {
    try {
      codexTarget = await resolveWslClientTarget("codex", codexExtensionPath, connection, wslRelay);
      if (!codexTarget) {
        throw new Error("Codex is configured to run in WSL, but no WSL Codex executable was found");
      }
    } catch (err) {
      codexResolutionFailed = true;
      logWarn("MCP", `Unable to configure Codex in its selected WSL host: ${errorMessage(err)}`);
      result.failed.push("Codex");
    }
  } else {
    codexTarget = await resolveClientTarget("codex", codexExtensionPath, connection, wslRelay);
  }
  if (codexTarget) {
    logInfo("MCP", `Using Codex CLI (${codexTarget.host}): ${codexTarget.cli.display}`);
    await configureCodex(codexTarget, result);
  } else if (!codexResolutionFailed) {
    const reason = codexExtensionPath ? "CLI executable not found" : "extension and CLI not found";
    logWarn("MCP", `Skipping Codex MCP configuration: ${reason}`);
    result.skipped.push(`Codex (${reason})`);
  }
  const claudeTarget = await resolveClientTarget("claude", claudeExtensionPath, connection, wslRelay);
  if (claudeTarget) {
    logInfo("MCP", `Using Claude Code CLI (${claudeTarget.host}): ${claudeTarget.cli.display}`);
    await configureClaude(claudeTarget, result);
  } else {
    const reason = claudeExtensionPath ? "CLI executable not found" : "extension and CLI not found";
    logWarn("MCP", `Skipping Claude Code MCP configuration: ${reason}`);
    result.skipped.push(`Claude Code (${reason})`);
  }
  return result;
}
async function configureCodex(target, result) {
  if (!target.configPath) throw new Error("Codex config path is unavailable");
  const addArgs = ["mcp", "add", MCP_SERVER_NAME, "--url", target.definition.url];
  const current = await runClientCommand(target.cli, ["mcp", "get", MCP_SERVER_NAME, "--json"]);
  if (current.code === 0 && isCurrentCodexEntry(current.stdout, target.definition)) {
    logInfo("MCP", `Codex MCP registration already verified in ${target.configPath}`);
    result.unchanged.push("Codex");
    return;
  }
  if (current.code === 0 && !isManagedCodexEntry(current.stdout)) {
    logWarn("MCP", `Codex server '${MCP_SERVER_NAME}' exists but is not managed by CanMV; leaving it unchanged`);
    result.skipped.push("Codex (name already in use)");
    return;
  }
  let preserveDisabled = false;
  if (current.code === 0) {
    try {
      preserveDisabled = await isCodexEntryExplicitlyDisabled(target);
    } catch (err) {
      failClient("Codex", `Unable to preserve Codex MCP state: ${errorMessage(err)}`, result);
      return;
    }
    const removed = await runClientCommand(target.cli, ["mcp", "remove", MCP_SERVER_NAME]);
    if (removed.code !== 0) {
      failClient("Codex", `Unable to update Codex MCP registration: ${cleanProcessError(removed)}`, result);
      return;
    }
  }
  const added = await runClientCommand(target.cli, addArgs);
  if (added.code !== 0) {
    failClient("Codex", `Unable to configure Codex MCP: ${cleanProcessError(added)}`, result);
    return;
  }
  try {
    await writeCodexHttpHeaders(target, target.definition.headers, preserveDisabled);
  } catch (err) {
    await runClientCommand(target.cli, ["mcp", "remove", MCP_SERVER_NAME]);
    failClient("Codex", `Unable to secure Codex MCP registration: ${errorMessage(err)}`, result);
    return;
  }
  const verified = await runClientCommand(target.cli, ["mcp", "get", MCP_SERVER_NAME, "--json"]);
  if (verified.code === 0 && isCurrentCodexEntry(verified.stdout, target.definition)) {
    const state = preserveDisabled ? " with its user-disabled state preserved" : "";
    logInfo("MCP", `Configured and verified CanMV Streamable HTTP server for Codex in ${target.configPath}${state}`);
    result.configured.push("Codex");
    return;
  }
  const detail = verified.code === 0 ? "saved registration does not match the requested CanMV HTTP configuration" : cleanProcessError(verified);
  failClient("Codex", `Codex registration verification failed: ${detail}`, result);
}
async function configureClaude(target, result) {
  const definition = JSON.stringify({
    type: "http",
    url: target.definition.url,
    headers: target.definition.headers
  });
  const addArgs = ["mcp", "add-json", "--scope", "user", MCP_SERVER_NAME, definition];
  const current = await runClientCommand(target.cli, ["mcp", "get", MCP_SERVER_NAME]);
  if (current.code === 0 && isCurrentClaudeEntry(current.stdout, target.definition)) {
    result.unchanged.push("Claude Code");
    return;
  }
  if (current.code === 0 && !isManagedEntry(current.stdout)) {
    logWarn("MCP", `Claude Code server '${MCP_SERVER_NAME}' exists but is not managed by CanMV; leaving it unchanged`);
    result.skipped.push("Claude Code (name already in use)");
    return;
  }
  if (current.code === 0) {
    const removeArgs = ["mcp", "remove", "--scope", "user", MCP_SERVER_NAME];
    const removed = await runClientCommand(target.cli, removeArgs);
    if (removed.code !== 0) {
      failClient("Claude Code", `Unable to update Claude Code MCP registration: ${cleanProcessError(removed)}`, result);
      return;
    }
  }
  const added = await runClientCommand(target.cli, addArgs);
  if (added.code !== 0) {
    failClient("Claude Code", `Unable to configure Claude Code MCP: ${cleanProcessError(added)}`, result);
    return;
  }
  const verified = await runClientCommand(target.cli, ["mcp", "get", MCP_SERVER_NAME]);
  if (verified.code === 0 && isSavedClaudeEntry(verified.stdout, target.definition)) {
    logInfo("MCP", "Configured and verified CanMV Streamable HTTP server for Claude Code");
    result.configured.push("Claude Code");
    return;
  }
  await runClientCommand(target.cli, ["mcp", "remove", "--scope", "user", MCP_SERVER_NAME]);
  const detail = verified.code === 0 ? "saved registration does not match the requested CanMV HTTP configuration" : cleanProcessError(verified);
  failClient("Claude Code", `Claude Code registration verification failed: ${detail}`, result);
}
function failClient(client, message, result) {
  logWarn("MCP", message);
  result.failed.push(client);
}
async function resolveClientTarget(executableName2, extensionPath, connection, wslRelay) {
  const nativeExecutable = findClientExecutable(extensionPath, executableName2);
  if (nativeExecutable) {
    return {
      cli: directClientCommand(nativeExecutable),
      host: "native",
      definition: managedHttpDefinition(connection.url, connection.token),
      configPath: executableName2 === "codex" ? nativeCodexConfigPath() : void 0
    };
  }
  return safelyResolveWslClientTarget(executableName2, extensionPath, connection, wslRelay);
}
async function safelyResolveWslClientTarget(executableName2, extensionPath, connection, wslRelay) {
  try {
    return await resolveWslClientTarget(executableName2, extensionPath, connection, wslRelay);
  } catch (err) {
    logWarn("MCP", `Unable to configure ${executableName2} in WSL: ${errorMessage(err)}`);
    return void 0;
  }
}
function shouldPreferWslCodex() {
  return process.platform === "win32" && vscode21.workspace.getConfiguration("chatgpt").get("runCodexInWindowsSubsystemForLinux", false);
}
async function resolveWslClientTarget(executableName2, extensionPath, connection, wslRelay) {
  if (process.platform !== "win32") return void 0;
  const wsl = windowsSystemExecutable("wsl.exe");
  const distro = await findWslDistro(wsl);
  let linuxExecutable;
  if (executableName2 === "codex" && extensionPath) {
    const bundled = findBundledLinuxExecutable(extensionPath, executableName2);
    if (bundled) linuxExecutable = await translateWindowsPathToWsl(wsl, distro, bundled);
  }
  linuxExecutable ||= await findWslExecutable(wsl, distro, executableName2);
  if (!linuxExecutable) return void 0;
  const relay = await wslRelay.start(wsl, distro, connection);
  logInfo("MCP", `Verified CanMV WSL HTTP relay in ${distro} at ${relay.url}`);
  const definition = managedHttpDefinition(relay.url, connection.token);
  const codexConfigPath = executableName2 === "codex" ? await resolveWslCodexConfigPath(wsl, distro) : void 0;
  return {
    cli: {
      executable: wsl,
      prefixArgs: ["-d", distro, "--", linuxExecutable],
      display: `${linuxExecutable} in WSL ${distro}`
    },
    host: "WSL",
    definition,
    configPath: codexConfigPath,
    wslConfig: codexConfigPath ? { wsl, distro, linuxPath: codexConfigPath } : void 0
  };
}
function managedHttpDefinition(url, token) {
  const authFingerprintHeader = AUTH_FINGERPRINT_HEADER_PREFIX + crypto3.createHash("sha256").update(token).digest("hex").slice(0, 16).toUpperCase();
  return {
    url,
    headers: {
      Authorization: `Bearer ${token}`,
      [MANAGED_HEADER]: MANAGED_MARKER,
      [authFingerprintHeader]: MANAGED_MARKER
    }
  };
}
function directClientCommand(executable) {
  return { executable, prefixArgs: [], display: executable };
}
function findClientExtensionPath(extensionId) {
  const visible = vscode21.extensions.getExtension(extensionId)?.extensionPath;
  if (visible) return visible;
  const roots = /* @__PURE__ */ new Set();
  if (process.env.VSCODE_EXTENSIONS) roots.add(process.env.VSCODE_EXTENSIONS);
  roots.add(path10.join(os5.homedir(), ".vscode", "extensions"));
  roots.add(path10.join(os5.homedir(), ".vscode-insiders", "extensions"));
  const prefix = `${extensionId.toLowerCase()}-`;
  const candidates = [];
  for (const root of roots) {
    try {
      for (const entry of fs9.readdirSync(root, { withFileTypes: true })) {
        if (entry.isDirectory() && entry.name.toLowerCase().startsWith(prefix)) {
          candidates.push(path10.join(root, entry.name));
        }
      }
    } catch {
    }
  }
  return candidates.sort((left, right) => right.localeCompare(left))[0];
}
function findClientExecutable(extensionPath, filename) {
  if (extensionPath) {
    const bundled = findFile(extensionPath, executableNames(filename), 5);
    if (bundled) return bundled;
  }
  const pathValue = process.env.PATH || "";
  for (const directory of pathValue.split(path10.delimiter)) {
    if (!directory) continue;
    for (const name of executableNames(filename)) {
      const candidate = path10.join(directory, name);
      if (isExecutableFile(candidate)) return candidate;
    }
  }
  return void 0;
}
function findBundledLinuxExecutable(extensionPath, filename) {
  return findExecutableInLinuxDirectory(path10.join(extensionPath, "bin"), filename, 3);
}
function findExecutableInLinuxDirectory(root, filename, depth) {
  if (depth < 0) return void 0;
  let entries;
  try {
    entries = fs9.readdirSync(root, { withFileTypes: true }).sort((left, right) => left.name.localeCompare(right.name));
  } catch {
    return void 0;
  }
  for (const entry of entries) {
    if (!entry.isDirectory()) continue;
    const directory = path10.join(root, entry.name);
    if (/^linux(?:-|$)/i.test(entry.name)) {
      const executable = findNamedFile(directory, filename, depth - 1);
      if (executable) return executable;
    }
    const nested = findExecutableInLinuxDirectory(directory, filename, depth - 1);
    if (nested) return nested;
  }
  return void 0;
}
function findFile(root, names, depth) {
  for (const name of names) {
    const found = findNamedFile(root, name, depth);
    if (found) return found;
  }
  return void 0;
}
function findNamedFile(root, name, depth) {
  if (depth < 0) return void 0;
  let entries;
  try {
    entries = fs9.readdirSync(root, { withFileTypes: true }).sort((left, right) => left.name.localeCompare(right.name));
  } catch {
    return void 0;
  }
  for (const entry of entries) {
    const matches = process.platform === "win32" ? entry.name.toLowerCase() === name.toLowerCase() : entry.name === name;
    if (entry.isFile() && matches) {
      const candidate = path10.join(root, entry.name);
      if (isExecutableFile(candidate)) return candidate;
    }
  }
  for (const entry of entries) {
    if (!entry.isDirectory()) continue;
    const found = findNamedFile(path10.join(root, entry.name), name, depth - 1);
    if (found) return found;
  }
  return void 0;
}
function executableNames(name) {
  return process.platform === "win32" ? [`${name}.exe`, `${name}.cmd`, `${name}.bat`] : [name];
}
function isExecutableFile(candidate) {
  try {
    fs9.accessSync(candidate, process.platform === "win32" ? fs9.constants.F_OK : fs9.constants.X_OK);
    return fs9.statSync(candidate).isFile();
  } catch {
    return false;
  }
}
async function findWslDistro(wsl) {
  const verbose = await runProcess(wsl, ["--list", "--verbose"]);
  if (verbose.code === 0) {
    const defaultLine = verbose.stdout.split(/\r?\n/).map((line) => line.trim()).find((line) => line.startsWith("*"));
    const defaultDistro = defaultLine?.slice(1).trim().split(/\s{2,}|\t+/)[0]?.trim();
    if (defaultDistro) return defaultDistro;
  }
  const listed = await runProcess(wsl, ["--list", "--quiet"]);
  if (listed.code !== 0) throw new Error(`unable to list WSL distributions: ${cleanProcessError(listed)}`);
  const distro = listed.stdout.split(/\r?\n/).map((line) => line.trim()).find(Boolean);
  if (!distro) throw new Error("no WSL distribution was found");
  return distro;
}
async function findWslExecutable(wsl, distro, name) {
  const command = await runProcess(wsl, [
    "-d",
    distro,
    "--",
    "/usr/bin/bash",
    "-lc",
    `command -v ${shellQuote(name)} 2>/dev/null || true`
  ]);
  const executable = command.stdout.trim().split(/\r?\n/)[0];
  return command.code === 0 && executable.startsWith("/") ? executable : void 0;
}
async function translateWindowsPathToWsl(wsl, distro, windowsPath) {
  const drivePath = windowsPath.match(/^([A-Za-z]):[\\/](.*)$/);
  if (drivePath) return `/mnt/${drivePath[1].toLowerCase()}/${drivePath[2].replace(/\\/g, "/")}`;
  const translated = await runProcess(wsl, ["-d", distro, "--", "wslpath", "-u", windowsPath.replace(/\\/g, "/")]);
  if (translated.code !== 0 || !translated.stdout.trim()) {
    throw new Error(`unable to translate path for WSL: ${cleanProcessError(translated)}`);
  }
  return translated.stdout.trim();
}
async function resolveWslCodexConfigPath(wsl, distro) {
  const codexHome = await runProcess(wsl, [
    "-d",
    distro,
    "--",
    "/usr/bin/printenv",
    "CODEX_HOME"
  ]);
  if (codexHome.code === 0 && codexHome.stdout.trim()) {
    return path10.posix.join(codexHome.stdout.trim(), "config.toml");
  }
  const home = await runProcess(wsl, [
    "-d",
    distro,
    "--",
    "/usr/bin/printenv",
    "HOME"
  ]);
  if (home.code !== 0 || !home.stdout.trim()) {
    throw new Error(`unable to resolve the WSL Codex home: ${cleanProcessError(home)}`);
  }
  return path10.posix.join(home.stdout.trim(), ".codex", "config.toml");
}
function windowsSystemExecutable(filename) {
  const systemRoot = process.env.SystemRoot || process.env.WINDIR || "C:\\Windows";
  return path10.win32.join(systemRoot, "System32", filename);
}
function windowsPowerShellExecutable() {
  return path10.win32.join(
    process.env.SystemRoot || process.env.WINDIR || "C:\\Windows",
    "System32",
    "WindowsPowerShell",
    "v1.0",
    "powershell.exe"
  );
}
function nativeCodexConfigPath() {
  const codexHome = process.env.CODEX_HOME || path10.join(os5.homedir(), ".codex");
  return path10.join(codexHome, "config.toml");
}
function isManagedEntry(output) {
  return output.includes(MANAGED_HEADER);
}
function isManagedCodexEntry(output) {
  const transport = codexTransport(output);
  return transport?.http_headers?.[MANAGED_HEADER] === MANAGED_MARKER || transport?.env?.CANMV_MCP_MANAGED === MANAGED_MARKER;
}
function isCurrentCodexEntry(output, definition) {
  const transport = codexTransport(output);
  return transport?.type === "streamable_http" && transport.url === definition.url && Object.entries(definition.headers).every(([key, value]) => transport.http_headers?.[key] === value);
}
function codexTransport(output) {
  try {
    const parsed = JSON.parse(output);
    const transport = parsed.transport;
    return transport && typeof transport === "object" && !Array.isArray(transport) ? transport : void 0;
  } catch {
    return void 0;
  }
}
function isCurrentClaudeEntry(output, definition) {
  return isSavedClaudeEntry(output, definition);
}
function isSavedClaudeEntry(output, definition) {
  return isManagedEntry(output) && /(?:^|\r?\n)\s*(?:Type|Transport):\s*(?:http|streamable[-_ ]http)\s*(?:\r?\n|$)/i.test(output) && outputIncludesValue(output, definition.url) && Object.keys(definition.headers).every((key) => outputIncludesValue(output, key));
}
function outputIncludesValue(output, value) {
  const jsonEscaped = JSON.stringify(value).slice(1, -1);
  return output.includes(value) || output.includes(jsonEscaped);
}
async function isCodexEntryExplicitlyDisabled(target) {
  if (!target.configPath) throw new Error("Codex config path is unavailable");
  const content = await readCodexConfig(target);
  const parsed = parse(content);
  const servers = parsed.mcp_servers;
  const entry = servers?.[MCP_SERVER_NAME];
  return entry?.enabled === false;
}
async function readCodexConfig(target) {
  if (!target.configPath) throw new Error("Codex config path is unavailable");
  if (target.wslConfig) return readWslFile(target.wslConfig);
  try {
    return fs9.readFileSync(target.configPath, "utf8");
  } catch (err) {
    throw new Error(`Unable to read Codex config ${target.configPath}: ${errorMessage(err)}`);
  }
}
async function writeCodexHttpHeaders(target, headers, disabled) {
  if (!target.configPath) throw new Error("Codex config path is unavailable");
  const original = await readCodexConfig(target);
  const updated = codexConfigWithHttpHeaders(original, target.configPath, headers, disabled);
  if (target.wslConfig) {
    await writeWslPrivateFile(target.wslConfig, updated);
  } else {
    writePrivateFileAtomic(target.configPath, updated);
  }
}
function codexConfigWithHttpHeaders(original, configPath, headers, disabled) {
  parse(original);
  const newline = original.includes("\r\n") ? "\r\n" : "\n";
  const lines = original.split(/\r?\n/);
  const headerPattern = /^\s*\[\s*mcp_servers\s*\.\s*(?:"canmv-k230"|'canmv-k230'|canmv-k230)\s*\]\s*(?:#.*)?$/;
  const tableStart = lines.findIndex((line) => headerPattern.test(line));
  if (tableStart < 0) throw new Error(`Codex did not create the expected MCP table in ${configPath}`);
  let tableEnd = tableStart + 1;
  while (tableEnd < lines.length && !/^\s*\[/.test(lines[tableEnd])) tableEnd += 1;
  const upsertTableValue = (pattern, value) => {
    const existing = lines.findIndex((line, index) => index > tableStart && index < tableEnd && pattern.test(line));
    if (existing >= 0) {
      lines[existing] = value;
      return;
    }
    while (tableEnd > tableStart + 1 && lines[tableEnd - 1].trim() === "") tableEnd -= 1;
    lines.splice(tableEnd, 0, value);
    tableEnd += 1;
  };
  upsertTableValue(/^\s*http_headers\s*=/, `http_headers = ${tomlInlineTable(headers)}`);
  if (disabled) upsertTableValue(/^\s*enabled\s*=/, "enabled = false");
  const updated = lines.join(newline);
  const parsed = parse(updated);
  const servers = parsed.mcp_servers;
  const entry = servers?.[MCP_SERVER_NAME];
  const savedHeaders = entry?.http_headers;
  if (!savedHeaders || Object.entries(headers).some(([key, value]) => savedHeaders[key] !== value)) {
    throw new Error("Generated Codex HTTP header configuration did not validate");
  }
  if (disabled && entry?.enabled !== false) {
    throw new Error("Generated Codex configuration did not preserve the user-disabled state");
  }
  return updated;
}
async function readWslFile(location) {
  const result = await runProcess(location.wsl, [
    "-d",
    location.distro,
    "--",
    "/bin/cat",
    "--",
    location.linuxPath
  ]);
  if (result.code !== 0) {
    throw new Error(`Unable to read WSL Codex config ${location.linuxPath}: ${cleanProcessError(result)}`);
  }
  return result.stdout;
}
function writePrivateFileAtomic(configPath, content) {
  let targetPath = configPath;
  try {
    if (fs9.lstatSync(configPath).isSymbolicLink()) {
      try {
        targetPath = fs9.realpathSync(configPath);
      } catch (err) {
        if (err.code !== "ENOENT") throw err;
        const linkTarget = fs9.readlinkSync(configPath);
        targetPath = path10.resolve(path10.dirname(configPath), linkTarget);
      }
    }
  } catch (err) {
    if (err.code !== "ENOENT") throw err;
  }
  const directory = path10.dirname(targetPath);
  fs9.mkdirSync(directory, { recursive: true, mode: 448 });
  const temporary = path10.join(directory, `.canmv-codex-config.${crypto3.randomBytes(16).toString("hex")}`);
  let descriptor;
  let moved = false;
  try {
    descriptor = fs9.openSync(temporary, "wx", 384);
    fs9.writeFileSync(descriptor, content, { encoding: "utf8" });
    fs9.fsyncSync(descriptor);
    fs9.closeSync(descriptor);
    descriptor = void 0;
    if (fs9.readFileSync(temporary, "utf8") !== content) {
      throw new Error("temporary Codex config verification failed");
    }
    fs9.renameSync(temporary, targetPath);
    moved = true;
    if (process.platform !== "win32") fs9.chmodSync(targetPath, 384);
    if (fs9.readFileSync(configPath, "utf8") !== content) {
      throw new Error("saved Codex config does not match the verified content");
    }
  } finally {
    if (descriptor !== void 0) fs9.closeSync(descriptor);
    if (!moved) {
      try {
        fs9.unlinkSync(temporary);
      } catch (err) {
        if (err.code !== "ENOENT") {
          logWarn("MCP", `Unable to remove temporary Codex config ${temporary}: ${errorMessage(err)}`);
        }
      }
    }
  }
}
async function writeWslPrivateFile(location, content) {
  if (!content) throw new Error("Refusing to replace the WSL Codex config with empty content");
  const resolved = await runProcess(location.wsl, [
    "-d",
    location.distro,
    "--",
    "/usr/bin/readlink",
    "-f",
    "--",
    location.linuxPath
  ]);
  const targetPath = resolved.code === 0 && resolved.stdout.trim() ? resolved.stdout.trim() : location.linuxPath;
  const directory = path10.posix.dirname(targetPath);
  const temporary = path10.posix.join(
    directory,
    `.canmv-codex-config.${crypto3.randomBytes(16).toString("hex")}`
  );
  const runWsl = (command, args = [], input) => runProcess(
    location.wsl,
    ["-d", location.distro, "--", command, ...args],
    input
  );
  let moved = false;
  try {
    const directoryResult = await runWsl("/bin/mkdir", ["-p", "--", directory]);
    if (directoryResult.code !== 0) {
      throw new Error(`unable to prepare the config directory: ${cleanProcessError(directoryResult)}`);
    }
    const touchResult = await runWsl("/usr/bin/touch", ["--", temporary]);
    if (touchResult.code !== 0) {
      throw new Error(`unable to create the temporary config: ${cleanProcessError(touchResult)}`);
    }
    const privateResult = await runWsl("/bin/chmod", ["600", "--", temporary]);
    if (privateResult.code !== 0) {
      throw new Error(`unable to secure the temporary config: ${cleanProcessError(privateResult)}`);
    }
    const writeResult = await runWsl("/usr/bin/tee", ["--", temporary], content);
    if (writeResult.code !== 0) {
      throw new Error(`unable to write the temporary config: ${cleanProcessError({ ...writeResult, stdout: "" })}`);
    }
    const temporaryContent = await readWslFile({ ...location, linuxPath: temporary });
    if (temporaryContent !== content) {
      throw new Error("temporary config verification failed; WSL stdin did not preserve the requested content");
    }
    const moveResult = await runWsl("/bin/mv", ["-f", "--", temporary, targetPath]);
    if (moveResult.code !== 0) {
      throw new Error(`unable to replace the config atomically: ${cleanProcessError(moveResult)}`);
    }
    moved = true;
    const savedContent = await readWslFile(location);
    if (savedContent !== content) {
      throw new Error("saved WSL Codex config does not match the verified content");
    }
  } catch (err) {
    throw new Error(`Unable to write WSL Codex config ${location.linuxPath}: ${errorMessage(err)}`);
  } finally {
    if (!moved) {
      const cleanupResult = await runWsl("/bin/rm", ["-f", "--", temporary]);
      if (cleanupResult.code !== 0) {
        logWarn("MCP", `Unable to remove temporary WSL Codex config ${temporary}: ${cleanProcessError(cleanupResult)}`);
      }
    }
  }
}
function tomlInlineTable(values) {
  return `{ ${Object.entries(values).map(([key, value]) => `${tomlKey(key)} = ${tomlString(value)}`).join(", ")} }`;
}
function tomlKey(value) {
  return /^[A-Za-z0-9_-]+$/.test(value) ? value : tomlString(value);
}
function runClientCommand(command, args) {
  const invocation = clientInvocation(command, args);
  return runProcess(invocation.executable, invocation.args);
}
function clientInvocation(command, args) {
  return { executable: command.executable, args: [...command.prefixArgs, ...args] };
}
function runProcess(executable, args, input) {
  return new Promise((resolve3) => {
    const invocation = processInvocation(executable, args);
    try {
      const child = cp2.execFile(invocation.executable, invocation.args, {
        encoding: "buffer",
        timeout: PROCESS_TIMEOUT_MS,
        maxBuffer: 1024 * 1024,
        windowsHide: true
      }, (err, stdout, stderr) => {
        const processError = err;
        const rawCode = processError?.code;
        resolve3({
          code: typeof rawCode === "number" ? rawCode : err ? 1 : 0,
          stdout: decodeProcessOutput(stdout),
          stderr: decodeProcessOutput(stderr),
          error: processErrorDescription(processError, executable)
        });
      });
      child.stdin?.on("error", () => void 0);
      child.stdin?.end(input);
    } catch (err) {
      resolve3({ code: 1, stdout: "", stderr: "", error: errorMessage(err) });
    }
  });
}
function decodeProcessOutput(value) {
  if (!value) return "";
  if (typeof value === "string") return value;
  if (value.length >= 2 && value[0] === 255 && value[1] === 254) return value.subarray(2).toString("utf16le");
  if (value.length >= 3 && value[0] === 239 && value[1] === 187 && value[2] === 191) {
    return value.subarray(3).toString("utf8");
  }
  const sampleLength = Math.min(value.length - value.length % 2, 256);
  let nullHighBytes = 0;
  for (let index = 1; index < sampleLength; index += 2) {
    if (value[index] === 0) nullHighBytes += 1;
  }
  return sampleLength > 0 && nullHighBytes / (sampleLength / 2) > 0.6 ? value.toString("utf16le").replace(/^\uFEFF/, "") : value.toString("utf8").replace(/^\uFEFF/, "");
}
function processInvocation(executable, args) {
  if (process.platform !== "win32" || !/\.(?:cmd|bat)$/i.test(executable)) return { executable, args };
  const powershell = windowsPowerShellExecutable();
  const script = [
    "[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)",
    "$OutputEncoding = [Console]::OutputEncoding",
    `& ${powershellQuote(executable)} ${args.map(powershellQuote).join(" ")}`,
    "if ($null -ne $LASTEXITCODE) { exit $LASTEXITCODE }"
  ].join("; ");
  return {
    executable: powershell,
    args: ["-NoLogo", "-NoProfile", "-NonInteractive", "-EncodedCommand", Buffer.from(script, "utf16le").toString("base64")]
  };
}
function processErrorDescription(err, executable) {
  if (!err) return void 0;
  if (err.killed) return `Process timed out after ${PROCESS_TIMEOUT_MS}ms`;
  if (err.signal) return `Process terminated by ${err.signal}`;
  if (typeof err.code === "string") return `${err.code}: unable to launch ${executable}`;
  if (typeof err.code === "number") return `Process exited with code ${err.code}`;
  return `Unable to launch ${executable}`;
}
function cleanProcessError(result) {
  return (result.stderr || result.stdout || result.error || `process exited with code ${result.code}`).trim().replace(/\b[0-9a-f]{64}\b/gi, "<redacted>").replace(/\b[A-Za-z0-9+/]{256,}={0,2}\b/g, "<redacted-command>").replace(/[\r\n]+/g, " ").slice(0, 500);
}
function powershellQuote(value) {
  return `'${value.replace(/'/g, "''")}'`;
}
function shellQuote(value) {
  return `'${value.replace(/'/g, `'"'"'`)}'`;
}
function tomlString(value) {
  return JSON.stringify(value);
}
function errorMessage(err) {
  return err instanceof Error ? err.message : String(err);
}

// src/mcp/httpService.ts
var cp3 = __toESM(require("child_process"));
var crypto4 = __toESM(require("crypto"));
var fs10 = __toESM(require("fs"));
var http2 = __toESM(require("http"));
var path11 = __toESM(require("path"));
var vscode22 = __toESM(require("vscode"));
var HTTP_PORT_STATE_KEY = "canmv.mcp.httpPort";
var HTTP_START_TIMEOUT_MS = 1e4;
var HTTP_STOP_TIMEOUT_MS = 5e3;
var HTTP_KILL_TIMEOUT_MS = 1e3;
var HTTP_RESTART_MAX_DELAY_MS = 1e4;
var McpHttpService = class {
  constructor(context, serverEnv) {
    this.context = context;
    this.serverEnv = serverEnv;
    this.restartAttempts = 0;
    this.expectedExits = /* @__PURE__ */ new WeakSet();
    this.connectionChanged = new vscode22.EventEmitter();
    this.disposed = false;
    this.onDidChangeConnection = this.connectionChanged.event;
  }
  start() {
    if (this.disposed) return Promise.reject(new Error("CanMV MCP HTTP service has been disposed"));
    if (this.connectionInfo) return Promise.resolve(this.connectionInfo);
    const operation = this.beginStart();
    void operation.catch(() => {
      const preferredPort = this.context.globalState.get(HTTP_PORT_STATE_KEY, 0);
      this.scheduleRestart(preferredPort);
    });
    return operation;
  }
  async restart() {
    if (this.disposed) throw new Error("CanMV MCP HTTP service has been disposed");
    if (this.startPromise) {
      try {
        await this.startPromise;
      } catch {
      }
    }
    const preferredPort = this.connectionInfo?.port || this.context.globalState.get(HTTP_PORT_STATE_KEY, 0);
    this.cancelScheduledRestart();
    this.setConnection(void 0);
    await this.stopChild();
    try {
      return await this.beginStart(preferredPort);
    } catch (err) {
      this.scheduleRestart(preferredPort);
      throw err;
    }
  }
  beginStart(preferredPort) {
    if (this.startPromise) return this.startPromise;
    const operation = this.startService(preferredPort).finally(() => {
      if (this.startPromise === operation) this.startPromise = void 0;
    });
    this.startPromise = operation;
    return operation;
  }
  async startService(preferredPort) {
    if (this.disposed) throw new Error("CanMV MCP HTTP service has been disposed");
    const token = readOrCreateHttpToken(this.context);
    const pkg = this.context.extension.packageJSON;
    const version2 = pkg.version || "unknown";
    const env3 = this.serverEnv();
    const configurationId = httpConfigurationId(env3);
    env3.CANMV_MCP_CONFIGURATION_ID = configurationId;
    const savedPort = validPort(preferredPort) || validPort(this.context.globalState.get(HTTP_PORT_STATE_KEY, 0));
    if (savedPort > 0 && await probeHttpService(savedPort, token, version2, configurationId)) {
      const existing = connectionInfo(savedPort, token);
      this.setConnection(existing);
      logInfo("MCP", `Using existing Streamable HTTP service at ${existing.url}`);
      return existing;
    }
    let started;
    try {
      started = await this.startChild(savedPort, token, env3);
    } catch (err) {
      if (savedPort <= 0) throw err;
      logWarn("MCP", `Unable to reuse MCP HTTP port ${savedPort}: ${errorMessage2(err)}; selecting a new port`);
      started = await this.startChild(0, token, env3);
    }
    if (this.child !== started.child || !childIsRunning(started.child)) {
      throw new Error("MCP HTTP service exited immediately after startup");
    }
    try {
      await this.context.globalState.update(HTTP_PORT_STATE_KEY, started.ready.port);
    } catch (err) {
      logWarn("MCP", `Unable to remember MCP HTTP port ${started.ready.port}: ${errorMessage2(err)}`);
    }
    if (this.child !== started.child || !childIsRunning(started.child)) {
      throw new Error("MCP HTTP service exited immediately after startup");
    }
    const connection = connectionInfo(started.ready.port, token);
    this.setConnection(connection);
    logInfo("MCP", `Streamable HTTP service ready at ${connection.url}`);
    return connection;
  }
  dispose() {
    if (this.disposed) return;
    this.disposed = true;
    this.cancelScheduledRestart();
    this.connectionInfo = void 0;
    this.connectionChanged.dispose();
    void this.stopChild();
  }
  startChild(port, token, serverEnv) {
    const serverPath = path11.join(this.context.extensionPath, "out", "mcp", "server.js");
    if (!fs10.existsSync(serverPath)) {
      return Promise.reject(new Error(`CanMV MCP server script not found: ${serverPath}`));
    }
    const listenHost = "127.0.0.1";
    return new Promise((resolve3, reject) => {
      const child = cp3.spawn(process.execPath, [serverPath], {
        cwd: this.context.extensionPath,
        stdio: ["ignore", "pipe", "pipe"],
        windowsHide: true,
        env: {
          ...process.env,
          ...stringEnvironment(serverEnv),
          ELECTRON_RUN_AS_NODE: "1",
          CANMV_MCP_HTTP_HOST: listenHost,
          CANMV_MCP_HTTP_PORT: String(port),
          CANMV_MCP_HTTP_TOKEN: token
        }
      });
      this.child = child;
      let settled = false;
      let stdout = "";
      const finish = (err, ready) => {
        if (settled) return;
        settled = true;
        clearTimeout(timer);
        if (err) {
          if (this.child === child) this.child = void 0;
          if (child.exitCode === null && child.signalCode === null) child.kill("SIGTERM");
          reject(err);
        } else if (ready) {
          resolve3({ child, ready });
        }
      };
      const timer = setTimeout(() => finish(new Error(`MCP HTTP service did not start within ${HTTP_START_TIMEOUT_MS}ms`)), HTTP_START_TIMEOUT_MS);
      timer.unref?.();
      child.stdout?.setEncoding("utf8");
      child.stdout?.on("data", (chunk) => {
        stdout += chunk;
        for (; ; ) {
          const newline = stdout.indexOf("\n");
          if (newline < 0) break;
          const line = stdout.slice(0, newline).trim();
          stdout = stdout.slice(newline + 1);
          if (!line) continue;
          try {
            const message = JSON.parse(line);
            if (message.type === "ready" && typeof message.port === "number" && message.port > 0) {
              finish(void 0, message);
            }
          } catch {
            logWarn("MCP", `Unexpected MCP HTTP service output: ${line.slice(0, 500)}`);
          }
        }
      });
      child.stderr?.setEncoding("utf8");
      child.stderr?.on("data", (chunk) => {
        const message = chunk.trim();
        if (message) logInfo("MCP Server", message);
      });
      child.once("error", (err) => finish(err));
      child.once("exit", (code, signal) => {
        const wasCurrent = this.child === child;
        if (wasCurrent) this.child = void 0;
        if (!settled) {
          finish(new Error(`MCP HTTP service exited before startup: code=${code ?? "null"} signal=${signal ?? "null"}`));
        } else if (!this.disposed && !this.expectedExits.has(child)) {
          logWarn("MCP", `Streamable HTTP service exited: code=${code ?? "null"} signal=${signal ?? "null"}`);
          if (wasCurrent && this.connectionInfo) {
            const preferredPort = this.connectionInfo.port;
            this.setConnection(void 0);
            this.scheduleRestart(preferredPort);
          }
        }
      });
    });
  }
  stopChild() {
    const child = this.child;
    this.child = void 0;
    if (!child || !childIsRunning(child)) return Promise.resolve();
    this.expectedExits.add(child);
    return new Promise((resolve3) => {
      let settled = false;
      let stopTimer;
      let killTimer;
      const finish = () => {
        if (settled) return;
        settled = true;
        if (stopTimer) clearTimeout(stopTimer);
        if (killTimer) clearTimeout(killTimer);
        resolve3();
      };
      stopTimer = setTimeout(() => {
        if (!childIsRunning(child)) {
          finish();
          return;
        }
        try {
          child.kill("SIGKILL");
        } catch {
          finish();
          return;
        }
        killTimer = setTimeout(finish, HTTP_KILL_TIMEOUT_MS);
        killTimer.unref?.();
      }, HTTP_STOP_TIMEOUT_MS);
      stopTimer.unref?.();
      child.once("exit", finish);
      if (!childIsRunning(child)) {
        finish();
        return;
      }
      try {
        child.kill("SIGTERM");
      } catch {
        finish();
      }
    });
  }
  scheduleRestart(preferredPort) {
    if (this.disposed || this.restartTimer) return;
    const delay = Math.min(500 * 2 ** this.restartAttempts, HTTP_RESTART_MAX_DELAY_MS);
    this.restartAttempts += 1;
    this.restartTimer = setTimeout(() => {
      this.restartTimer = void 0;
      void this.beginStart(preferredPort).catch((err) => {
        logWarn("MCP", `Unable to restart Streamable HTTP service: ${errorMessage2(err)}`);
        this.scheduleRestart(preferredPort);
      });
    }, delay);
    this.restartTimer.unref?.();
  }
  cancelScheduledRestart() {
    if (!this.restartTimer) return;
    clearTimeout(this.restartTimer);
    this.restartTimer = void 0;
  }
  setConnection(connection) {
    const previous = this.connectionInfo;
    if (previous?.url === connection?.url && previous?.token === connection?.token) return;
    this.connectionInfo = connection;
    if (connection) this.restartAttempts = 0;
    this.connectionChanged.fire(connection);
  }
};
function connectionInfo(port, token) {
  return {
    url: `http://127.0.0.1:${port}/mcp`,
    port,
    token,
    headers: { Authorization: `Bearer ${token}` }
  };
}
function stringEnvironment(env3) {
  const result = {};
  for (const [key, value] of Object.entries(env3)) {
    if (value !== null) result[key] = String(value);
  }
  return result;
}
function readOrCreateHttpToken(context) {
  const storagePath = context.globalStorageUri.fsPath;
  const tokenPath = path11.join(storagePath, "mcp-http-token");
  fs10.mkdirSync(storagePath, { recursive: true, mode: 448 });
  try {
    const token2 = fs10.readFileSync(tokenPath, "utf8").trim();
    if (/^[0-9a-f]{64}$/i.test(token2)) {
      if (process.platform !== "win32") fs10.chmodSync(tokenPath, 384);
      return token2;
    }
  } catch (err) {
    if (err.code !== "ENOENT") throw err;
  }
  const token = crypto4.randomBytes(32).toString("hex");
  try {
    fs10.writeFileSync(tokenPath, token + "\n", { encoding: "utf8", flag: "wx", mode: 384 });
    return token;
  } catch (err) {
    if (err.code !== "EEXIST") throw err;
    const existing = fs10.readFileSync(tokenPath, "utf8").trim();
    if (/^[0-9a-f]{64}$/i.test(existing)) {
      if (process.platform !== "win32") fs10.chmodSync(tokenPath, 384);
      return existing;
    }
    throw new Error(`Invalid CanMV MCP HTTP token file: ${tokenPath}`);
  }
}
function probeHttpService(port, token, version2, configurationId) {
  return new Promise((resolve3) => {
    const request3 = http2.request({
      host: "127.0.0.1",
      port,
      path: "/health",
      method: "GET",
      headers: { Authorization: `Bearer ${token}` },
      timeout: 500
    }, (response) => {
      const chunks = [];
      response.on("data", (chunk) => chunks.push(chunk));
      response.on("end", () => {
        try {
          const body = JSON.parse(Buffer.concat(chunks).toString("utf8"));
          resolve3(response.statusCode === 200 && body.service === "canmv-k230" && body.transport === "streamable-http" && body.version === version2 && body.configurationId === configurationId);
        } catch {
          resolve3(false);
        }
      });
    });
    request3.once("timeout", () => {
      request3.destroy();
      resolve3(false);
    });
    request3.once("error", () => resolve3(false));
    request3.end();
  });
}
function httpConfigurationId(env3) {
  const entries = Object.entries(env3).sort(([left], [right]) => left.localeCompare(right));
  return crypto4.createHash("sha256").update(JSON.stringify(entries)).digest("hex");
}
function validPort(value) {
  return typeof value === "number" && Number.isInteger(value) && value > 0 && value <= 65535 ? value : 0;
}
function childIsRunning(child) {
  return child.exitCode === null && child.signalCode === null;
}
function errorMessage2(err) {
  return err instanceof Error ? err.message : String(err);
}

// src/mcp/wslHttpRelay.ts
var cp4 = __toESM(require("child_process"));
var fs11 = __toESM(require("fs"));
var http3 = __toESM(require("http"));
var path12 = __toESM(require("path"));
var RELAY_START_TIMEOUT_MS = 1e4;
var RELAY_REQUEST_TIMEOUT_MS = 12e4;
var MAX_RELAY_BODY_BYTES = 32 * 1024 * 1024;
var WSL_RELAY_PORT_STATE_KEY_PREFIX = "canmv.mcp.wslRelayPort.";
var HOP_BY_HOP_HEADERS = /* @__PURE__ */ new Set([
  "connection",
  "content-length",
  "host",
  "keep-alive",
  "proxy-authenticate",
  "proxy-authorization",
  "te",
  "trailer",
  "transfer-encoding",
  "upgrade"
]);
var WslHttpRelayService = class {
  constructor(context) {
    this.context = context;
    this.instances = /* @__PURE__ */ new Map();
    this.starts = /* @__PURE__ */ new Map();
    this.startingChildren = /* @__PURE__ */ new Set();
    this.generation = 0;
    this.disposed = false;
  }
  start(wsl, distro, upstream) {
    if (this.disposed) return Promise.reject(new Error("CanMV WSL HTTP relay has been disposed"));
    const existing = this.instances.get(distro);
    if (existing) return Promise.resolve(existing.connection);
    const pending = this.starts.get(distro);
    if (pending) return pending;
    const generation = this.generation;
    let start;
    start = this.startWithSavedPort(wsl, distro, upstream, generation).finally(() => {
      if (this.starts.get(distro) === start) this.starts.delete(distro);
    });
    this.starts.set(distro, start);
    return start;
  }
  reset() {
    if (this.disposed) return;
    this.generation += 1;
    this.stopInstances();
    this.starts.clear();
  }
  dispose() {
    if (this.disposed) return;
    this.disposed = true;
    this.generation += 1;
    this.stopInstances();
    this.starts.clear();
  }
  stopInstances() {
    for (const instance of this.instances.values()) {
      stopRelayChild(instance.child);
    }
    for (const child of this.startingChildren) stopRelayChild(child);
    this.instances.clear();
    this.startingChildren.clear();
  }
  async startWithSavedPort(wsl, distro, upstream, generation) {
    const stateKey = relayPortStateKey(distro);
    const savedPort = validPort2(this.context.globalState.get(stateKey, 0));
    let connection;
    try {
      connection = await this.startRelay(wsl, distro, upstream, savedPort, generation);
    } catch (err) {
      if (this.disposed || this.generation !== generation) throw err;
      if (savedPort === 0) throw err;
      logWarn("MCP", `Unable to reuse WSL HTTP relay port ${savedPort} in ${distro}: ${errorMessage3(err)}; selecting a new port`);
      connection = await this.startRelay(wsl, distro, upstream, 0, generation);
    }
    if (this.disposed || this.generation !== generation) {
      throw new Error("CanMV WSL HTTP relay startup was superseded");
    }
    try {
      await this.context.globalState.update(stateKey, connection.port);
    } catch (err) {
      logWarn("MCP", `Unable to remember WSL HTTP relay port ${connection.port} for ${distro}: ${errorMessage3(err)}`);
    }
    return connection;
  }
  async startRelay(wsl, distro, upstream, port, generation) {
    const relayPath = await this.resolveRelayPath(wsl, distro);
    if (this.disposed || this.generation !== generation) {
      throw new Error("CanMV WSL HTTP relay startup was superseded");
    }
    return new Promise((resolve3, reject) => {
      const child = cp4.spawn(wsl, ["-d", distro, "--", relayPath, "--http-relay", String(port)], {
        stdio: ["pipe", "pipe", "pipe"],
        windowsHide: true
      });
      this.startingChildren.add(child);
      let settled = false;
      let relayConnection;
      const finish = (err, connection) => {
        if (settled) return;
        settled = true;
        clearTimeout(timer);
        if (err) {
          this.startingChildren.delete(child);
          stopRelayChild(child);
          reject(err);
          return;
        }
        if (!connection) return;
        if (this.disposed || this.generation !== generation) {
          this.startingChildren.delete(child);
          stopRelayChild(child);
          reject(new Error("CanMV WSL HTTP relay startup was superseded"));
          return;
        }
        this.startingChildren.delete(child);
        const instance = { child, connection };
        this.instances.set(distro, instance);
        resolve3(connection);
      };
      const timer = setTimeout(() => {
        finish(new Error(`WSL HTTP relay did not start within ${RELAY_START_TIMEOUT_MS}ms`));
      }, RELAY_START_TIMEOUT_MS);
      timer.unref?.();
      const reader = new FramedMessageReader({
        onMessage: (message) => {
          if (!isEvent(message)) return;
          const params = asObject(message.params);
          if (message.event === "httpRelayReady") {
            const port2 = integerValue(params.port);
            if (port2 > 0 && port2 <= 65535) {
              if (relayConnection) return;
              relayConnection = { url: `http://127.0.0.1:${port2}/mcp`, port: port2 };
              logInfo("MCP", `WSL HTTP relay listening in ${distro} at ${relayConnection.url}; validating end-to-end forwarding`);
              if (!writeRelayMessage(child, "httpRelaySelfTest", { token: upstream.token })) {
                finish(new Error("Unable to start the WSL HTTP relay self-test"));
              }
            }
            return;
          }
          if (message.event === "httpRelaySelfTestResult") {
            if (!relayConnection) {
              finish(new Error("WSL HTTP relay returned a self-test result before reporting its address"));
              return;
            }
            if (params.ok === true) {
              finish(void 0, relayConnection);
              return;
            }
            const statusCode = integerValue(params.statusCode);
            const detail = typeof params.error === "string" && params.error.trim() ? params.error.trim() : "unknown relay error";
            const status = statusCode > 0 ? ` (HTTP ${statusCode})` : "";
            finish(new Error(`WSL HTTP relay self-test failed${status}: ${detail}`));
            return;
          }
          if (message.event === "httpRelayRequest") {
            void this.forwardRequest(child, upstream, params);
          }
        },
        onFrame: () => void 0
      });
      child.stdout?.on("data", (chunk) => reader.handleData(chunk));
      child.stderr?.setEncoding("utf8");
      child.stderr?.on("data", (chunk) => {
        const message = chunk.trim();
        if (message) logInfo("MCP Relay", message);
      });
      child.stdin?.on("error", (err) => {
        if (!settled) finish(new Error(`WSL HTTP relay input failed: ${errorMessage3(err)}`));
      });
      child.once("error", (err) => finish(err));
      child.once("exit", (code, signal) => {
        this.startingChildren.delete(child);
        const current = this.instances.get(distro);
        if (current?.child === child) this.instances.delete(distro);
        if (!settled) {
          finish(new Error(`WSL HTTP relay exited before startup: code=${code ?? "null"} signal=${signal ?? "null"}`));
        } else if (!this.disposed && this.generation === generation) {
          logWarn("MCP", `WSL HTTP relay in ${distro} exited: code=${code ?? "null"} signal=${signal ?? "null"}`);
        }
      });
    });
  }
  async resolveRelayPath(wsl, distro) {
    const architecture = (await runWslText(wsl, distro, ["uname", "-m"])).toLowerCase();
    const target = /^(?:x86_64|amd64)$/.test(architecture) ? "linux-x64" : /^(?:aarch64|arm64)$/.test(architecture) ? "linux-arm64" : void 0;
    if (!target) throw new Error(`Unsupported WSL relay architecture: ${architecture || "<unknown>"}`);
    const windowsPath = path12.join(this.context.extensionPath, "bin", target, "canmv-backend");
    if (!fs11.existsSync(windowsPath)) {
      throw new Error(`CanMV WSL HTTP relay executable not found: ${windowsPath}`);
    }
    const drivePath = windowsPath.match(/^([A-Za-z]):[\\/](.*)$/);
    if (drivePath) return `/mnt/${drivePath[1].toLowerCase()}/${drivePath[2].replace(/\\/g, "/")}`;
    const translated = await runWslText(wsl, distro, ["wslpath", "-u", windowsPath]);
    if (!translated) throw new Error(`Unable to translate WSL relay path: ${windowsPath}`);
    return translated;
  }
  async forwardRequest(child, upstream, params) {
    const requestId = integerValue(params.requestId);
    let request3;
    try {
      request3 = parseRelayRequest(params);
    } catch (err) {
      logWarn("MCP", `Rejected invalid WSL relay request: ${errorMessage3(err)}`);
      if (requestId > 0) {
        writeRelayResponse(
          child,
          requestId,
          400,
          { "Content-Type": ["application/json; charset=utf-8"] },
          Buffer.from(JSON.stringify({ error: "Invalid CanMV relay request" }))
        );
      }
      return;
    }
    try {
      const response = await requestUpstream(upstream.port, request3);
      writeRelayResponse(child, request3.requestId, response.statusCode, response.headers, response.body);
    } catch (err) {
      logWarn("MCP", `WSL HTTP relay upstream failed: ${errorMessage3(err)}`);
      writeRelayResponse(
        child,
        request3.requestId,
        502,
        { "Content-Type": ["application/json; charset=utf-8"] },
        Buffer.from(JSON.stringify({ error: "CanMV MCP upstream is unavailable" }))
      );
    }
  }
};
function parseRelayRequest(params) {
  const requestId = integerValue(params.requestId);
  const method = typeof params.method === "string" ? params.method : "";
  const requestPath = typeof params.path === "string" ? params.path : "";
  if (!Number.isSafeInteger(requestId) || requestId <= 0) throw new Error("invalid request ID");
  let pathname = "";
  try {
    pathname = new URL(requestPath, "http://localhost").pathname;
  } catch {
    throw new Error("invalid request path");
  }
  if (!(method === "GET" && pathname === "/health" || method === "POST" && pathname === "/mcp")) {
    throw new Error("unsupported route or method");
  }
  const bodyBase64 = typeof params.bodyBase64 === "string" ? params.bodyBase64 : "";
  const body = Buffer.from(bodyBase64, "base64");
  if (body.byteLength > MAX_RELAY_BODY_BYTES) throw new Error("request body is too large");
  return {
    requestId,
    method,
    path: requestPath,
    headers: parseHeaders(params.headers),
    body
  };
}
function requestUpstream(port, relayRequest) {
  return new Promise((resolve3, reject) => {
    const request3 = http3.request({
      host: "127.0.0.1",
      port,
      method: relayRequest.method,
      path: relayRequest.path,
      headers: relayRequest.headers,
      timeout: RELAY_REQUEST_TIMEOUT_MS
    }, (response) => {
      const chunks = [];
      let byteLength = 0;
      response.on("data", (chunk) => {
        byteLength += chunk.byteLength;
        if (byteLength > MAX_RELAY_BODY_BYTES) {
          response.destroy(new Error("CanMV MCP relay response is too large"));
          return;
        }
        chunks.push(chunk);
      });
      response.once("error", reject);
      response.on("end", () => resolve3({
        statusCode: response.statusCode || 502,
        headers: responseHeaders(response.headers),
        body: Buffer.concat(chunks)
      }));
    });
    request3.once("timeout", () => request3.destroy(new Error("CanMV MCP relay upstream timed out")));
    request3.once("error", reject);
    request3.end(relayRequest.body);
  });
}
function writeRelayResponse(child, requestId, statusCode, headers, body) {
  writeRelayMessage(child, "httpRelayResponse", {
    requestId,
    statusCode,
    headers,
    bodyBase64: body.toString("base64")
  });
}
function writeRelayMessage(child, method, params) {
  if (!child.stdin?.writable) return false;
  const payload = Buffer.from(JSON.stringify({
    id: 0,
    method,
    params
  }));
  const header = Buffer.alloc(7);
  MAGIC.copy(header, 0);
  header[2] = MSG_REQUEST;
  header.writeUInt32LE(payload.byteLength, 3);
  try {
    child.stdin.write(Buffer.concat([header, payload]));
    return true;
  } catch {
    return false;
  }
}
function parseHeaders(value) {
  const result = {};
  for (const [key, raw] of Object.entries(asObject(value))) {
    if (!relayHeaderAllowed(key) || !Array.isArray(raw)) continue;
    const values = raw.filter((item) => typeof item === "string");
    if (values.length > 0) result[key] = values;
  }
  return result;
}
function responseHeaders(headers) {
  const result = {};
  for (const [key, value] of Object.entries(headers)) {
    if (!relayHeaderAllowed(key) || value === void 0) continue;
    result[key] = Array.isArray(value) ? value : [value];
  }
  return result;
}
function relayHeaderAllowed(key) {
  return !HOP_BY_HOP_HEADERS.has(key.toLowerCase());
}
function runWslText(wsl, distro, command) {
  return new Promise((resolve3) => {
    cp4.execFile(wsl, ["-d", distro, "--", ...command], {
      encoding: "utf8",
      timeout: RELAY_START_TIMEOUT_MS,
      windowsHide: true
    }, (err, stdout) => resolve3(err ? "" : stdout.trim()));
  });
}
function integerValue(value) {
  return typeof value === "number" && Number.isInteger(value) ? value : 0;
}
function validPort2(value) {
  return Number.isInteger(value) && value > 0 && value <= 65535 ? value : 0;
}
function stopRelayChild(child) {
  child.stdin?.end();
  if (child.exitCode === null && child.signalCode === null) child.kill("SIGTERM");
}
function relayPortStateKey(distro) {
  return `${WSL_RELAY_PORT_STATE_KEY_PREFIX}${encodeURIComponent(distro.toLowerCase())}`;
}
function asObject(value) {
  return value && typeof value === "object" && !Array.isArray(value) ? value : {};
}
function errorMessage3(err) {
  return err instanceof Error ? err.message : String(err);
}

// src/mcp/provider.ts
var CANMV_MCP_PROVIDER_ID = "canmv.mcp";
async function registerMcpSupport(context, bridge) {
  removeLegacyMcpManualSetup(context);
  const changed = new vscode23.EventEmitter();
  const httpService = new McpHttpService(context, () => createMcpServerEnv(context, bridge));
  const wslRelay = new WslHttpRelayService(context);
  context.subscriptions.push(httpService, wslRelay);
  let httpConnection;
  try {
    httpConnection = await httpService.start();
  } catch (err) {
    logWarn("MCP", `Streamable HTTP service unavailable: ${err instanceof Error ? err.message : String(err)}`);
  }
  const showClientConfigurationFailure = () => {
    void vscode23.window.showErrorMessage(
      t("CanMV: MCP client configuration failed. See the CanMV output for details.")
    );
  };
  const showClientConfigurationSuccess = (names) => {
    void vscode23.window.showInformationMessage(
      t("CanMV: MCP configured for {clients}.", {
        clients: names.join(", ")
      })
    );
  };
  let configurePromise;
  let configureConnection;
  let configureAgain = false;
  const configureClients = (showResult) => {
    const connection = httpConnection;
    if (!connection) return Promise.reject(new Error("CanMV MCP HTTP service is unavailable"));
    if (!configurePromise) {
      configureConnection = connection;
      configurePromise = configureExternalMcpClients(connection, wslRelay).finally(() => {
        configurePromise = void 0;
        configureConnection = void 0;
        if (configureAgain) {
          configureAgain = false;
          queueMicrotask(configureAutomatically);
        }
      });
    } else if (configureConnection?.url !== connection.url || configureConnection?.token !== connection.token) {
      configureAgain = true;
    }
    if (showResult) {
      void configurePromise.then((result) => {
        const names = [...result.configured, ...result.unchanged];
        if (result.failed.length > 0) {
          showClientConfigurationFailure();
        } else if (names.length > 0) {
          showClientConfigurationSuccess(names);
        } else if (result.missingExtensions.length === Object.keys(MCP_CLIENT_EXTENSION_IDS).length) {
          const openExtensions = t("Open Extensions");
          const showConfiguration = t("Show MCP Configuration");
          void vscode23.window.showErrorMessage(
            t("CanMV: Install the Codex or Claude Code extension before configuring MCP."),
            openExtensions,
            showConfiguration
          ).then((choice) => {
            if (choice === openExtensions) {
              void vscode23.commands.executeCommand(
                "workbench.extensions.action.showExtensionsWithIds",
                Object.values(MCP_CLIENT_EXTENSION_IDS)
              );
            } else if (choice === showConfiguration) {
              void showManualConfiguration();
            }
          });
        } else {
          const showConfiguration = t("Show MCP Configuration");
          void vscode23.window.showWarningMessage(
            t("CanMV: No supported Codex or Claude Code client was found."),
            showConfiguration
          ).then((choice) => {
            if (choice === showConfiguration) void showManualConfiguration();
          });
        }
      }, (err) => {
        logWarn("MCP", `Client configuration failed: ${err instanceof Error ? err.message : String(err)}`);
        vscode23.window.showErrorMessage(t("CanMV: MCP client configuration failed. See the CanMV output for details."));
      });
    }
    return configurePromise;
  };
  const configureAutomatically = (force = false) => {
    if (!httpConnection || !shouldAutoConfigureExternalClients()) return;
    if (force && configurePromise) configureAgain = true;
    void configureClients(false).then((result) => {
      if (result.failed.length > 0) showClientConfigurationFailure();
    }, (err) => {
      logWarn("MCP", `Automatic client configuration failed: ${err instanceof Error ? err.message : String(err)}`);
    });
  };
  const showManualConfiguration = async () => {
    const connection = httpConnection;
    if (!connection) {
      showClientConfigurationFailure();
      return;
    }
    const document = await vscode23.workspace.openTextDocument({
      language: "markdown",
      content: manualMcpConfiguration(connection)
    });
    await vscode23.window.showTextDocument(document, { preview: true });
  };
  let lastRelayUpstreamUrl = httpConnection?.url;
  const connectionSubscription = httpService.onDidChangeConnection((connection) => {
    if (connection && lastRelayUpstreamUrl && connection.url !== lastRelayUpstreamUrl) {
      wslRelay.reset();
    }
    if (connection) lastRelayUpstreamUrl = connection.url;
    httpConnection = connection;
    changed.fire();
    if (connection) configureAutomatically();
  });
  let restartPromise;
  let restartAgain = false;
  const restartHttpService = () => {
    if (restartPromise) {
      restartAgain = true;
      return;
    }
    restartPromise = (async () => {
      do {
        restartAgain = false;
        try {
          await httpService.restart();
        } catch (err) {
          logWarn("MCP", `Unable to refresh Streamable HTTP service settings: ${err instanceof Error ? err.message : String(err)}`);
        }
      } while (restartAgain);
    })().finally(() => {
      restartPromise = void 0;
      if (restartAgain) restartHttpService();
    });
  };
  const configSubscription = vscode23.workspace.onDidChangeConfiguration((event) => {
    if (event.affectsConfiguration("canmv.baudRate") || event.affectsConfiguration("canmv.autoMinifyStartupScripts")) {
      restartHttpService();
    }
    if (event.affectsConfiguration("chatgpt.runCodexInWindowsSubsystemForLinux")) {
      wslRelay.reset();
      changed.fire();
      configureAutomatically(true);
    }
  });
  context.subscriptions.push(
    changed,
    connectionSubscription,
    configSubscription,
    vscode23.commands.registerCommand("canmv.configureMcpClients", () => configureClients(true)),
    vscode23.commands.registerCommand("canmv.showMcpConfiguration", showManualConfiguration)
  );
  configureAutomatically();
  const registerProvider = vscode23.lm?.registerMcpServerDefinitionProvider;
  if (typeof registerProvider !== "function" || typeof vscode23.McpHttpServerDefinition !== "function") {
    logWarn("MCP", "VS Code Streamable HTTP MCP server definition API is unavailable in this runtime");
    return;
  }
  const provider = {
    onDidChangeMcpServerDefinitions: changed.event,
    provideMcpServerDefinitions: () => {
      const connection = httpConnection;
      if (!connection) return [];
      const pkg = context.extension.packageJSON;
      const version2 = pkg.version || "unknown";
      return [new vscode23.McpHttpServerDefinition(
        "CanMV K230",
        vscode23.Uri.parse(connection.url),
        connection.headers,
        version2
      )];
    },
    resolveMcpServerDefinition: (server) => {
      const connection = httpConnection;
      if (!connection) throw new Error("CanMV MCP HTTP service is unavailable");
      server.uri = vscode23.Uri.parse(connection.url);
      server.headers = connection.headers;
      return server;
    }
  };
  context.subscriptions.push(
    registerProvider(CANMV_MCP_PROVIDER_ID, provider)
  );
  logInfo("MCP", "Registered CanMV Streamable HTTP server definition provider");
}
function createMcpServerEnv(context, bridge) {
  const config = vscode23.workspace.getConfiguration("canmv");
  const pkg = context.extension.packageJSON;
  const baudRate = config.get("baudRate", 12e6);
  const autoMinifyStartupScripts = config.get("autoMinifyStartupScripts", true);
  const env3 = {
    CANMV_EXTENSION_PATH: context.extensionPath,
    CANMV_EXTENSION_VERSION: pkg.version || "unknown",
    CANMV_BAUD_RATE: Number.isFinite(baudRate) ? baudRate : 12e6,
    CANMV_AUTO_MINIFY_STARTUP_SCRIPTS: autoMinifyStartupScripts ? "true" : "false"
  };
  if (bridge) {
    env3.CANMV_MCP_BRIDGE_ENDPOINT = bridge.endpoint;
    env3.CANMV_MCP_BRIDGE_TOKEN = bridge.token;
    env3.CANMV_MCP_BRIDGE_REQUIRED = "true";
  }
  return env3;
}
function shouldAutoConfigureExternalClients() {
  return vscode23.workspace.getConfiguration("canmv").get("mcp.autoConfigureClients", true);
}
function manualMcpConfiguration(connection) {
  const definition = {
    type: "http",
    url: connection.url,
    headers: connection.headers
  };
  const json = JSON.stringify({
    mcpServers: {
      "canmv-k230": definition
    }
  }, null, 2);
  const headers = Object.entries(connection.headers).map(([key, value]) => `${JSON.stringify(key)} = ${JSON.stringify(value)}`).join(", ");
  return [
    `# ${t("CanMV MCP configuration")}`,
    "",
    `> ${t("Keep this bearer token private. The endpoint is available only while the CanMV extension is running.")}`,
    "",
    "## JSON",
    "",
    "```json",
    json,
    "```",
    "",
    "## TOML",
    "",
    "```toml",
    "[mcp_servers.canmv-k230]",
    `url = ${JSON.stringify(connection.url)}`,
    `http_headers = { ${headers} }`,
    "```",
    ""
  ].join("\n");
}

// src/mcp/bridge.ts
var crypto5 = __toESM(require("crypto"));
var fs12 = __toESM(require("fs"));
var net = __toESM(require("net"));
var os6 = __toESM(require("os"));
var path13 = __toESM(require("path"));
var MAX_BRIDGE_MESSAGE_BYTES = 16 * 1024 * 1024;
var McpBridgeServer = class {
  constructor(context, handleRequest, getSnapshot) {
    this.context = context;
    this.handleRequest = handleRequest;
    this.getSnapshot = getSnapshot;
    this.server = net.createServer((socket) => this.accept(socket));
    this.clients = /* @__PURE__ */ new Set();
    this.requestQueue = Promise.resolve();
    this.ownsEndpoint = false;
    this.disposed = false;
  }
  async start() {
    if (this.connectionInfo) return this.connectionInfo;
    if (this.disposed) throw new Error("CanMV MCP bridge has been disposed");
    const endpoint = createBridgeEndpoint(this.context);
    const token = readOrCreateBridgeToken(this.context);
    if (await isBridgeEndpointActive(endpoint)) {
      this.connectionInfo = { endpoint, token };
      logInfo("MCP", `Using existing local bridge at ${endpoint}`);
      return this.connectionInfo;
    }
    if (process.platform !== "win32" && fs12.existsSync(endpoint)) {
      fs12.unlinkSync(endpoint);
    }
    await this.listen(endpoint);
    if (process.platform !== "win32") {
      fs12.chmodSync(endpoint, 384);
    }
    this.ownsEndpoint = true;
    this.connectionInfo = { endpoint, token };
    logInfo("MCP", `Local bridge listening at ${endpoint}`);
    return this.connectionInfo;
  }
  listen(endpoint) {
    return new Promise((resolve3, reject) => {
      const onError = (err) => {
        this.server.off("listening", onListening);
        reject(err);
      };
      const onListening = () => {
        this.server.off("error", onError);
        resolve3();
      };
      this.server.once("error", onError);
      this.server.once("listening", onListening);
      this.server.listen(endpoint);
    });
  }
  broadcastEvent(event) {
    if (event.event === "frameAvailable") {
      const params = event.params;
      const data = params.data ? Buffer.from(asUint8Array(params.data)) : Buffer.alloc(0);
      this.broadcast({
        type: "frame",
        frameId: params.frameId || 0,
        dataBase64: data.toString("base64"),
        chunkTs: params.chunkTs,
        dispatchTs: params.dispatchTs
      });
      return;
    }
    this.broadcast({ type: "event", event });
  }
  broadcastSnapshot() {
    this.broadcast({
      type: "event",
      event: { event: "mcpBridgeSnapshot", params: this.getSnapshot() }
    });
  }
  dispose() {
    if (this.disposed) return;
    this.disposed = true;
    for (const client of this.clients) {
      client.socket.destroy();
    }
    this.clients.clear();
    if (this.ownsEndpoint) this.server.close();
    const endpoint = this.connectionInfo?.endpoint;
    this.connectionInfo = void 0;
    if (this.ownsEndpoint && endpoint && process.platform !== "win32") {
      try {
        fs12.unlinkSync(endpoint);
      } catch (err) {
        if (err.code !== "ENOENT") {
          logDebug("MCP", `Unable to remove bridge socket: ${errorMessage4(err)}`);
        }
      }
    }
    this.ownsEndpoint = false;
  }
  accept(socket) {
    socket.setEncoding("utf8");
    const client = { socket, authenticated: false, buffer: "", queue: Promise.resolve() };
    this.clients.add(client);
    socket.on("data", (chunk) => this.handleData(client, chunk));
    socket.on("error", (err) => logDebug("MCP", `Bridge client error: ${err.message}`));
    socket.on("close", () => this.clients.delete(client));
  }
  handleData(client, chunk) {
    client.buffer += chunk;
    if (Buffer.byteLength(client.buffer) > MAX_BRIDGE_MESSAGE_BYTES) {
      logWarn("MCP", "Closing bridge client after oversized message");
      client.socket.destroy();
      return;
    }
    for (; ; ) {
      const newline = client.buffer.indexOf("\n");
      if (newline < 0) return;
      const line = client.buffer.slice(0, newline).trim();
      client.buffer = client.buffer.slice(newline + 1);
      if (!line) continue;
      let message;
      try {
        message = JSON.parse(line);
      } catch {
        this.write(client, { type: "error", message: "Invalid bridge JSON" });
        client.socket.destroy();
        return;
      }
      client.queue = client.queue.then(() => this.handleMessage(client, message)).catch((err) => {
        logWarn("MCP", `Bridge request failed: ${errorMessage4(err)}`);
      });
    }
  }
  async handleMessage(client, message) {
    if (!client.authenticated) {
      if (message.type !== "hello" || message.token !== this.connectionInfo?.token) {
        this.write(client, { type: "hello", ok: false });
        client.socket.destroy();
        return;
      }
      client.authenticated = true;
      this.write(client, { type: "hello", ok: true, snapshot: this.getSnapshot() });
      return;
    }
    if (message.type !== "request" || typeof message.id !== "number" || typeof message.method !== "string") {
      this.write(client, { type: "error", message: "Invalid bridge request" });
      return;
    }
    const id = message.id;
    const params = asObject2(message.params);
    try {
      const operation = this.requestQueue.then(() => this.handleRequest(message.method, params));
      this.requestQueue = operation.then(() => void 0, () => void 0);
      const response = await operation;
      this.write(client, { type: "response", response: { ...response, id } });
    } catch (err) {
      this.write(client, {
        type: "response",
        response: { id, error: { code: 9001, message: errorMessage4(err) } }
      });
    }
  }
  broadcast(message) {
    for (const client of this.clients) {
      if (client.authenticated) this.write(client, message);
    }
  }
  write(client, message) {
    if (client.socket.writable) {
      client.socket.write(JSON.stringify(message) + "\n");
    }
  }
};
function createBridgeEndpoint(context) {
  const id = crypto5.createHash("sha256").update(context.globalStorageUri.fsPath).digest("hex").slice(0, 20);
  if (process.platform === "win32") {
    return `\\\\.\\pipe\\canmv-mcp-${id}`;
  }
  return path13.join(os6.tmpdir(), `canmv-mcp-${id}.sock`);
}
function readOrCreateBridgeToken(context) {
  const storagePath = context.globalStorageUri.fsPath;
  const tokenPath = path13.join(storagePath, "mcp-bridge-token");
  fs12.mkdirSync(storagePath, { recursive: true });
  try {
    const token2 = fs12.readFileSync(tokenPath, "utf8").trim();
    if (/^[0-9a-f]{64}$/i.test(token2)) {
      if (process.platform !== "win32") fs12.chmodSync(tokenPath, 384);
      return token2;
    }
  } catch (err) {
    if (err.code !== "ENOENT") throw err;
  }
  const token = crypto5.randomBytes(32).toString("hex");
  try {
    fs12.writeFileSync(tokenPath, token + "\n", { encoding: "utf8", flag: "wx", mode: 384 });
    return token;
  } catch (err) {
    if (err.code !== "EEXIST") throw err;
    const existing = fs12.readFileSync(tokenPath, "utf8").trim();
    if (/^[0-9a-f]{64}$/i.test(existing)) {
      if (process.platform !== "win32") fs12.chmodSync(tokenPath, 384);
      return existing;
    }
    throw new Error(`Invalid CanMV MCP bridge token file: ${tokenPath}`);
  }
}
function isBridgeEndpointActive(endpoint) {
  return new Promise((resolve3) => {
    const socket = net.createConnection(endpoint);
    const finish = (active) => {
      socket.removeAllListeners();
      socket.destroy();
      resolve3(active);
    };
    const timer = setTimeout(() => finish(false), 250);
    timer.unref?.();
    socket.once("connect", () => {
      clearTimeout(timer);
      finish(true);
    });
    socket.once("error", () => {
      clearTimeout(timer);
      finish(false);
    });
  });
}
function asObject2(value) {
  return value && typeof value === "object" && !Array.isArray(value) ? value : {};
}
function asUint8Array(value) {
  return value instanceof Uint8Array ? value : new Uint8Array(value);
}
function errorMessage4(err) {
  return err instanceof Error ? err.message : String(err);
}

// src/extension.ts
var disposables = [];
var previewPanel;
var thresholdEditorPanel;
var terminalViewProvider;
var backend;
var stubsService;
var examplesService;
var canmvResourceService;
var statusItem = vscode24.window.createStatusBarItem(vscode24.StatusBarAlignment.Left, 100);
statusItem.text = "$(debug-disconnect) CanMV";
statusItem.tooltip = states.disconnected();
var scriptExceptionBufferLimit = 4096;
var tracebackHeader = "Traceback (most recent call last):";
var pythonExceptionLinePattern = /^(?:[A-Za-z_][A-Za-z0-9_]*\.)*[A-Za-z_][A-Za-z0-9_]*(?::.*)?$/;
var ignoredStopExceptionLinePattern = /^(?:KeyboardInterrupt|SystemExit)(?::.*)?$/;
var ideInterruptExceptionLine = "Exception: IDE interrupt";
function scriptExceptionSummary(output, stopInFlight = false) {
  const tracebackIndex = output.lastIndexOf(tracebackHeader);
  if (tracebackIndex < 0) return void 0;
  const lines = output.slice(tracebackIndex + tracebackHeader.length).split(/\r?\n/);
  for (let i = lines.length - 1; i >= 0; i--) {
    const rawLine = lines[i];
    const line = rawLine.trim();
    if (!line || /^\s/.test(rawLine) || line.startsWith("File ") || line.startsWith("Traceback ") || /^\^+$/.test(line)) {
      continue;
    }
    if (ignoredStopExceptionLinePattern.test(line) || stopInFlight && line === ideInterruptExceptionLine) {
      return void 0;
    }
    if (pythonExceptionLinePattern.test(line)) {
      return line;
    }
  }
  return void 0;
}
async function activate(context) {
  logActivationInfo(context);
  backend = new NativeBackend(context);
  const session = new Session(backend, {
    autoReconnect: vscode24.workspace.getConfiguration("canmv").get("autoReconnect", true),
    requestTimeout: 1e4
  });
  context.subscriptions.push(session);
  const resourceRouteService = new CanmvResourceRouteService();
  examplesService = new ExamplesService(context, resourceRouteService);
  stubsService = new StubsService(context, resourceRouteService);
  canmvResourceService = new CanmvResourceService(resourceRouteService, stubsService, examplesService);
  void canmvResourceService.ensureDefaultResources().catch((err) => {
    logError("Resources", `Default setup error: ${err}`);
  });
  context.subscriptions.push(statusItem);
  const boardService = new BoardService(session, new BoardDetector(session));
  const scriptService = new ScriptService(session);
  const fileService = new FileService(
    session,
    () => vscode24.workspace.getConfiguration("canmv").get("autoMinifyStartupScripts", true)
  );
  const pkg = context.extension.packageJSON;
  const extensionName = pkg.displayName || pkg.name || "CanMV";
  const extensionVersion = pkg.version || "unknown";
  const extensionStatusLabel = `${extensionName} v${extensionVersion}`;
  let remoteFilesAvailable = () => false;
  let remoteFilesUnavailableMessage = () => t("Not connected");
  const remoteMirrorService = new RemoteMirrorService(
    context,
    fileService,
    () => remoteFilesAvailable(),
    () => remoteFilesUnavailableMessage()
  );
  const examplesTreeProvider = new ExamplesTreeProvider(examplesService);
  context.subscriptions.push(vscode24.window.registerTreeDataProvider("canmv.examples", examplesTreeProvider));
  let connected = false;
  let disconnected = true;
  let scriptRunning = false;
  let boardReady = false;
  let pendingBoardReadyEvent = false;
  let connectionBusy = false;
  let connectionPhase = "idle";
  let scriptBusy = false;
  let scriptStopInFlight = false;
  let scriptExceptionBuffer = "";
  let scriptExceptionNotified = false;
  let lastOperationEndTime = 0;
  let remoteFilesPausedUntil = 0;
  let remoteFilesPauseTimer;
  let controlProvider;
  let explorer;
  let explorerRefreshTimer;
  let updateExplorerConnectionState = () => {
  };
  let refreshExplorerSoon = () => {
  };
  let pauseRemoteFiles = () => {
  };
  let onScriptRunningContextChanged = () => {
  };
  let mcpBridge;
  const extensionStatusTooltipLines = () => [
    t("CanMV extension"),
    t("Extension Version: {version}", { version: extensionVersion })
  ];
  const statusTooltipForState = (state) => [
    ...extensionStatusTooltipLines(),
    t("Status: {status}", { status: state })
  ].join("\n");
  const setStatusForState = (state) => {
    if (state === "connecting") {
      statusItem.text = `$(sync~spin) ${extensionStatusLabel}`;
      statusItem.tooltip = statusTooltipForState(states.connecting());
      return;
    }
    if (state === "streaming") {
      statusItem.text = `$(device-camera) ${extensionStatusLabel}`;
      statusItem.tooltip = statusTooltipForState(states.streaming());
      return;
    }
    if (state === "connected") {
      statusItem.text = `$(debug-start) ${extensionStatusLabel}`;
      statusItem.tooltip = statusTooltipForState(states.connected());
      return;
    }
    statusItem.text = `$(debug-disconnect) ${extensionStatusLabel}`;
    statusItem.tooltip = statusTooltipForState(states.disconnected());
  };
  const boardStatusLabel = (info) => {
    const board = info.boardName || info.boardType;
    return [board, info.fwVersion, info.memorySize].filter(Boolean).join(" ") || "CanMV";
  };
  const sidebarStatusText = (state) => {
    if (state === "connecting") return states.connecting();
    if (state === "streaming") return states.streaming();
    if (state === "connected") {
      const info = boardService.boardInfo();
      return info ? boardStatusLabel(info) : states.connected();
    }
    return states.disconnected();
  };
  const setBoardReadyContext = (value) => {
    boardReady = value;
    void vscode24.commands.executeCommand("setContext", "canmv.boardReady", value);
    controlProvider?.setState({ boardReady: value });
    updateTerminalInputState();
    updateExplorerConnectionState();
    if (value) {
      refreshExplorerSoon(250);
    }
    mcpBridge?.broadcastSnapshot();
  };
  const resetBoardReadiness = () => {
    pendingBoardReadyEvent = false;
    setBoardReadyContext(false);
  };
  const markBoardReadyEvent = () => {
    pendingBoardReadyEvent = true;
    if (boardService.boardInfo()) {
      setBoardReadyContext(true);
    }
  };
  const setConnectionBusyContext = (value) => {
    connectionBusy = value;
    void vscode24.commands.executeCommand("setContext", "canmv.connectionBusy", value);
    updateTerminalInputState();
    updateExplorerConnectionState();
    if (!value) {
      refreshExplorerSoon(250);
    }
  };
  const setConnectionPhase = (value) => {
    connectionPhase = value;
    controlProvider?.setState({ connectionPhase: value });
  };
  const resetScriptExceptionDetector = () => {
    scriptExceptionBuffer = "";
    scriptExceptionNotified = false;
  };
  const inspectScriptOutputForException = (text) => {
    if (!scriptRunning || scriptExceptionNotified) return;
    scriptExceptionBuffer = (scriptExceptionBuffer + text).slice(-scriptExceptionBufferLimit);
    const summary = scriptExceptionSummary(scriptExceptionBuffer, scriptStopInFlight);
    if (!summary) return;
    scriptExceptionNotified = true;
    logWarn("Script", `Runtime exception detected: ${summary}`);
    const showTerminal = t("Show Terminal");
    void vscode24.window.showWarningMessage(t("CanMV: Script exception detected - {message}", { message: summary }), showTerminal).then((selection) => {
      if (selection === showTerminal) {
        void vscode24.commands.executeCommand("canmv.terminalView.focus");
      }
    });
  };
  const setScriptBusyContext = (value) => {
    scriptBusy = value;
    void vscode24.commands.executeCommand("setContext", "canmv.scriptBusy", value);
    updateTerminalInputState();
    updateExplorerConnectionState();
  };
  const beginScriptOperation = (options = {}) => {
    if (scriptBusy || connectionBusy && !options.allowWhileConnectionBusy) return false;
    if (!options.skipCooldown) {
      const cooldownMs = 500;
      const elapsed = Date.now() - lastOperationEndTime;
      if (elapsed < cooldownMs) {
        logDebug("Script", `Operation deferred: cooldown ${cooldownMs - elapsed}ms remaining`);
        return false;
      }
    }
    setScriptBusyContext(true);
    return true;
  };
  const endScriptOperation = () => {
    lastOperationEndTime = Date.now();
    setScriptBusyContext(false);
  };
  const setConnectionContexts = (state) => {
    if (state === "connecting") {
      setConnectionPhase("connecting");
    } else if (state === "disconnected") {
      setConnectionPhase("idle");
    }
    connected = state === "connected" || state === "streaming";
    disconnected = state === "disconnected";
    if (state === "connecting" || state === "disconnected") {
      resetBoardReadiness();
    }
    void vscode24.commands.executeCommand("setContext", "canmv.connected", connected);
    void vscode24.commands.executeCommand("setContext", "canmv.disconnected", disconnected);
    controlProvider?.setState({ connected, statusText: sidebarStatusText(state) });
    updateTerminalInputState();
    updateExplorerConnectionState();
  };
  const setScriptRunningContext = (value) => {
    const wasRunning = scriptRunning;
    scriptRunning = value;
    // ready means the REPL/debug side is idle.  A running Production or debug
    // script therefore always owns the board, even if a stale boardReady event
    // arrived during the preceding soft reboot.
    if (value && boardReady) {
      setBoardReadyContext(false);
    }
    if (!wasRunning && value) {
      resetScriptExceptionDetector();
    }
    if (wasRunning && !value) {
      pauseRemoteFiles(1500);
      resetScriptExceptionDetector();
    }
    void vscode24.commands.executeCommand("setContext", "canmv.scriptRunning", value);
    previewPanel?.sendScriptRunning(value);
    controlProvider?.setState({ scriptRunning: value });
    updateTerminalInputState();
    updateExplorerConnectionState();
    onScriptRunningContextChanged();
    mcpBridge?.broadcastSnapshot();
  };
  const boardStatusText = (_info) => {
    return `$(circuit-board) ${extensionStatusLabel}`;
  };
  const boardStatusTooltip = (info) => {
    const board = info.boardName || info.boardType;
    const stateLabel = session.state === "streaming" ? states.streaming() : states.connected();
    const lines = [
      ...extensionStatusTooltipLines(),
      t("Status: {status}", { status: stateLabel }),
      "",
      t("CanMV board connected"),
      t("Board: {board}", { board }),
      t("Firmware: {firmwareVersion}", { firmwareVersion: info.fwVersion })
    ];
    if (info.memorySize) lines.push(t("Memory: {memory}", { memory: info.memorySize }));
    if (info.port) lines.push(t("Port: {port}", { port: info.port }));
    return lines.join("\n");
  };
  const updateBoardStatus = () => {
    const info = boardService.boardInfo();
    if (info) {
      statusItem.text = boardStatusText(info);
      statusItem.tooltip = boardStatusTooltip(info);
      controlProvider?.setState({ statusText: boardStatusLabel(info) });
      return;
    }
    setStatusForState(session.state);
  };
  setStatusForState(session.state);
  statusItem.show();
  const boardSupportsReplInput = () => {
    return boardService.boardInfo()?.capabilities?.replInput === true;
  };
  const boardSupportsFileExplorer = () => {
    return boardService.boardInfo()?.capabilities?.listDir === true;
  };
  const boardHasCapabilitiesProtocol = () => {
    return (boardService.boardInfo()?.protocolVersion ?? 0) > 0;
  };
  const syncBoardExecutionState = async () => {
    if (!connected || !boardService.boardInfo()) return false;
    if (!boardHasCapabilitiesProtocol()) {
      setScriptRunningContext(false);
      setBoardReadyContext(true);
      return true;
    }
    const runningResult = await session.request(createRequest(Methods.scriptRunning, {}));
    if (!isResponse(runningResult)) return false;
    const running = runningResult.result.running === true;
    setScriptRunningContext(running);
    setBoardReadyContext(!running);
    return true;
  };
  const assumeScriptRunningForPreview = () => {
    return scriptRunning || !boardHasCapabilitiesProtocol();
  };
  remoteFilesAvailable = () => {
    return connected && boardReady && !connectionBusy && !scriptBusy && Date.now() >= remoteFilesPausedUntil && boardSupportsFileExplorer();
  };
  remoteFilesUnavailableMessage = () => {
    if (!connected) return t("Not connected");
    if (!boardReady) return t("Board is not ready yet");
    if (connectionBusy || scriptBusy) return t("CanMV operation is in progress");
    if (Date.now() < remoteFilesPausedUntil) return t("CanMV operation is in progress");
    return t("File explorer is not supported by this firmware");
  };
  pauseRemoteFiles = (durationMs) => {
    remoteFilesPausedUntil = Math.max(remoteFilesPausedUntil, Date.now() + durationMs);
    updateExplorerConnectionState();
    if (remoteFilesPauseTimer) {
      clearTimeout(remoteFilesPauseTimer);
    }
    const remainingMs = Math.max(0, remoteFilesPausedUntil - Date.now());
    remoteFilesPauseTimer = setTimeout(() => {
      remoteFilesPauseTimer = void 0;
      updateExplorerConnectionState();
    }, remainingMs);
  };
  const explorerCanBrowse = () => remoteFilesAvailable();
  updateExplorerConnectionState = () => {
    const filesAvailable = remoteFilesAvailable();
    void vscode24.commands.executeCommand("setContext", "canmv.remoteFilesAvailable", filesAvailable);
    const activeExplorer = explorer;
    if (!activeExplorer) return;
    const canDisplayFiles = connected && boardReady && boardSupportsFileExplorer();
    activeExplorer.setConnectionState(canDisplayFiles, !connected || boardSupportsFileExplorer(), remoteFilesUnavailableMessage());
  };
  const updateTerminalInputState = () => {
    const replInputSupported = boardSupportsReplInput();
    const canInput = connected && boardReady && replInputSupported && !scriptRunning && !connectionBusy && !scriptBusy;
    const reason = disconnected ? t("Connect board to use REPL input") : connectionBusy || scriptBusy ? t("CanMV operation is in progress") : !boardReady ? t("Board is not ready yet") : !replInputSupported ? t("REPL input is not supported by this firmware") : scriptRunning ? t("Script is running; press Ctrl-C to stop it") : "";
    terminalViewProvider?.setInputEnabled(canInput, reason, connected && scriptRunning);
  };
  setConnectionContexts(session.state);
  setScriptRunningContext(false);
  setConnectionBusyContext(false);
  setScriptBusyContext(false);
  setBoardReadyContext(false);
  let previewManuallyStopped = false;
  let previewPausedForScript = false;
  let previewAutoStartInFlight = false;
  let previewAutoStartPromise;
  let previewAutoStartToken = 0;
  let previewAutoRetryTimer;
  let previewAutoRetryCount = 0;
  let previewWatchdogTimer;
  let previewRecoverInFlight = false;
  let virtualTouchState = { supported: false, enabled: false };
  let virtualTouchRefreshTimer;
  let virtualTouchRefreshInFlight = false;
  const previewFrameStaleMs = 4e3;
  const previewWatchdogIntervalMs = 1500;
  const virtualTouchFrameStaleMs = 3e3;
  const virtualTouchRefreshIntervalMs = 2e3;
  const terminalScrollback = [];
  const terminalScrollbackLimit = 128 * 1024;
  let terminalScrollbackSize = 0;
  const trimTerminalScrollback = () => {
    while (terminalScrollbackSize > terminalScrollbackLimit && terminalScrollback.length > 0) {
      const excess = terminalScrollbackSize - terminalScrollbackLimit;
      const first = terminalScrollback[0] || "";
      if (first.length <= excess) {
        const removed = terminalScrollback.shift() || "";
        terminalScrollbackSize -= removed.length;
      } else {
        terminalScrollback[0] = first.slice(excess);
        terminalScrollbackSize -= excess;
      }
    }
  };
  const appendTerminal = (text) => {
    if (!text) return;
    terminalScrollback.push(text);
    terminalScrollbackSize += text.length;
    trimTerminalScrollback();
    inspectScriptOutputForException(text);
    terminalViewProvider?.appendText(text);
  };
  const appendTerminalLine = (text) => {
    appendTerminal(`${text}
`);
  };
  const sendVirtualTouchState = (state = virtualTouchState) => {
    previewPanel?.sendVirtualTouchState(state);
  };
  const setVirtualTouchState = (state) => {
    virtualTouchState = {
      supported: state.supported === true,
      enabled: state.supported === true && state.enabled === true,
      range: state.range,
      queueDepth: state.queueDepth
    };
    sendVirtualTouchState();
  };
  const clearVirtualTouchState = () => {
    setVirtualTouchState({ supported: false, enabled: false });
  };
  const boardSupportsVirtualTouch = () => {
    return boardService.boardInfo()?.capabilities?.virtualTouch === true;
  };
  const refreshVirtualTouchState = async () => {
    if (virtualTouchRefreshInFlight) {
      return;
    }
    if (!connected || !scriptRunning || session.state !== "streaming") {
      clearVirtualTouchState();
      return;
    }
    if (!boardSupportsVirtualTouch()) {
      clearVirtualTouchState();
      return;
    }
    const frameAge = videoService?.lastFrameAgeMs();
    if (frameAge === null || frameAge === void 0 || frameAge > virtualTouchFrameStaleMs) {
      clearVirtualTouchState();
      return;
    }
    virtualTouchRefreshInFlight = true;
    try {
      const result = await session.request(createRequest(Methods.virtualTouchStatus, {}));
      if (!isResponse(result)) {
        clearVirtualTouchState();
        return;
      }
      if (!connected || !scriptRunning || session.state !== "streaming" || !boardSupportsVirtualTouch()) {
        clearVirtualTouchState();
        return;
      }
      setVirtualTouchState(result.result);
    } finally {
      virtualTouchRefreshInFlight = false;
    }
  };
  const updateVirtualTouchRefreshTimer = () => {
    const shouldPoll = connected && scriptRunning && session.state === "streaming" && boardSupportsVirtualTouch() && !!previewPanel && !previewPanel.disposed;
    if (shouldPoll && !virtualTouchRefreshTimer) {
      virtualTouchRefreshTimer = setInterval(() => {
        void refreshVirtualTouchState().catch((err) => {
          logDebug("Touch", `Status refresh failed: ${err instanceof Error ? err.message : String(err)}`);
        });
      }, virtualTouchRefreshIntervalMs);
    } else if (!shouldPoll && virtualTouchRefreshTimer) {
      clearInterval(virtualTouchRefreshTimer);
      virtualTouchRefreshTimer = void 0;
    }
  };
  const sendVirtualTouchTap = async (tap) => {
    if (!virtualTouchState.enabled || !connected || !scriptRunning || session.state !== "streaming") {
      return;
    }
    const frameAge = videoService?.lastFrameAgeMs();
    if (frameAge === null || frameAge === void 0 || frameAge > virtualTouchFrameStaleMs) {
      clearVirtualTouchState();
      return;
    }
    const base = {
      x: Math.round(tap.x),
      y: Math.round(tap.y),
      sourceWidth: Math.round(tap.sourceWidth),
      sourceHeight: Math.round(tap.sourceHeight),
      trackId: 1,
      width: 1
    };
    const down = await session.request(createRequest(Methods.virtualTouchEvent, { ...base, event: "down" }));
    if (!isResponse(down) || !down.result.accepted) {
      if (boardSupportsVirtualTouch()) {
        await refreshVirtualTouchState();
      }
      return;
    }
    const up = await session.request(createRequest(Methods.virtualTouchEvent, { ...base, event: "up" }));
    if (!isResponse(up) || !up.result.accepted) {
      if (boardSupportsVirtualTouch()) {
        await refreshVirtualTouchState();
      }
    }
  };
  onScriptRunningContextChanged = () => {
    updatePreviewWatchdog();
    updateVirtualTouchRefreshTimer();
    if (!scriptRunning) {
      cancelPreviewAutoStart();
      clearVirtualTouchState();
    }
  };
  context.subscriptions.push(new vscode24.Disposable(() => {
    if (virtualTouchRefreshTimer) {
      clearInterval(virtualTouchRefreshTimer);
      virtualTouchRefreshTimer = void 0;
    }
    clearPreviewWatchdog();
  }));
  const registry = new ToolRegistry();
  registry.register({
    id: "preview",
    name: t("Preview"),
    icon: "device-camera",
    factory: () => {
      if (previewPanel?.disposed) {
        logInfo("Preview", "Recreating panel after disposal");
        videoService = void 0;
      }
      previewPanel = new PreviewPanel(context);
      previewPanel.onDidDispose(() => {
        logInfo("Preview", "Panel disposed by VS Code");
        if (videoService) {
          const disposedVideoService = videoService;
          void stopPreviewAfterScript().finally(() => {
            if (videoService === disposedVideoService) {
              videoService = void 0;
            }
            if (previewPanel && !previewPanel.disposed && scriptRunning && !previewManuallyStopped) {
              schedulePreviewAuto(150);
            }
          });
        }
        cancelPreviewAutoStart();
        previewPanel = void 0;
        updateVirtualTouchRefreshTimer();
        clearVirtualTouchState();
      });
      previewPanel.onCommand(async (command) => {
        if (command === "setPreviewDisabled") {
          await setPreviewDisabledManual(true);
        } else if (command === "setPreviewEnabled") {
          await setPreviewDisabledManual(false);
        } else if (command === "stopScript") {
          await vscode24.commands.executeCommand("canmv.stopScript");
        } else if (command === "disconnectBoard") {
          await vscode24.commands.executeCommand("canmv.disconnectBoard");
        }
      });
      previewPanel.onSaveImage(async (data) => {
        const stamp = (/* @__PURE__ */ new Date()).toISOString().replace(/[:.]/g, "-");
        const base = vscode24.workspace.workspaceFolders?.[0]?.uri;
        const defaultUri = base ? vscode24.Uri.joinPath(base, `canmv-frame-${stamp}.png`) : void 0;
        const target = await vscode24.window.showSaveDialog({
          defaultUri,
          filters: { [t("PNG Image")]: ["png"] },
          saveLabel: t("Save Image")
        });
        if (!target) return;
        await vscode24.workspace.fs.writeFile(target, data);
        logInfo("Preview", `Saved frame image: ${target.fsPath}`);
      });
      previewPanel.onSaveVideo(async ({ data, extension }) => {
        const normalizedExtension = extension === "mp4" ? "mp4" : "webm";
        const stamp = (/* @__PURE__ */ new Date()).toISOString().replace(/[:.]/g, "-");
        const base = vscode24.workspace.workspaceFolders?.[0]?.uri;
        const defaultUri = base ? vscode24.Uri.joinPath(base, `canmv-recording-${stamp}.${normalizedExtension}`) : void 0;
        const filterName = normalizedExtension === "mp4" ? t("MP4 Video") : t("WebM Video");
        const target = await vscode24.window.showSaveDialog({
          defaultUri,
          filters: { [filterName]: [normalizedExtension] },
          saveLabel: t("Save Video")
        });
        if (!target) return;
        await vscode24.workspace.fs.writeFile(target, data);
        logInfo("Preview", `Saved video recording: ${target.fsPath}`);
      });
      previewPanel.onVirtualTouch((tap) => {
        void sendVirtualTouchTap(tap).catch((err) => {
          logDebug("Touch", `Tap failed: ${err instanceof Error ? err.message : String(err)}`);
        });
      });
      const info = boardService.boardInfo();
      if (info) {
        previewPanel.sendBoardInfo(info);
      }
      previewPanel.sendPreviewDisabled(previewManuallyStopped);
      previewPanel.sendScriptRunning(scriptRunning);
      previewPanel.sendState(session.state);
      sendVirtualTouchState();
      updateVirtualTouchRefreshTimer();
      setTimeout(() => {
        if (previewPanel && !previewPanel.disposed && scriptRunning && !previewManuallyStopped) {
          schedulePreviewAuto(150);
        }
      }, 0);
      return previewPanel;
    }
  });
  registry.register({
    id: "thresholdEditor",
    name: t("Threshold Editor"),
    icon: "settings",
    factory: () => {
      if (thresholdEditorPanel?.disposed) {
        thresholdEditorPanel = void 0;
      }
      thresholdEditorPanel = new ThresholdEditorPanel(context);
      thresholdEditorPanel.onDidDispose(() => {
        thresholdEditorPanel = void 0;
      });
      thresholdEditorPanel.onCopyThreshold((text) => {
        void vscode24.env.clipboard.writeText(text).then(() => {
          thresholdEditorPanel?.sendCopied();
        });
      });
      thresholdEditorPanel.onApplyThreshold((text) => {
        void applyThresholdToSelection(text).catch((err) => {
          vscode24.window.showErrorMessage(t("CanMV: Failed to update threshold - {message}", { message: err instanceof Error ? err.message : String(err) }));
        });
      });
      thresholdEditorPanel.onRequestPreviewFrame(() => {
        const frame = videoService?.getLatestFrame();
        if (frame) {
          thresholdEditorPanel?.sendPreviewFrame(frame);
          return;
        }
        if (!previewPanel) {
          thresholdEditorPanel?.sendFrameUnavailable(t("No frame buffer image available. Start Preview, wait for a frame, or open an image file."));
          return;
        }
        void previewPanel.captureImage().then((data) => {
          if (data) {
            thresholdEditorPanel?.sendPreviewFrame(data, t("Preview Canvas"));
          } else {
            thresholdEditorPanel?.sendFrameUnavailable(t("No frame buffer image available. Start Preview, wait for a frame, or open an image file."));
          }
        });
      });
      thresholdEditorPanel.configure(createThresholdEditorConfig());
      return thresholdEditorPanel;
    }
  });
  const toolHost = new ToolHost(registry);
  let videoService;
  const getVideoService = () => {
    const b = backend;
    if (!videoService && previewPanel) {
      videoService = new VideoService(session, b, previewPanel);
      videoService.onFirstFrame(() => {
        updateVirtualTouchRefreshTimer();
        void refreshVirtualTouchState().catch((err) => {
          logDebug("Touch", `Status refresh failed: ${err instanceof Error ? err.message : String(err)}`);
        });
      });
    }
    return videoService;
  };
  const ensurePreviewPanel = () => {
    if (!previewPanel) {
      toolHost.open("preview");
    }
    return previewPanel;
  };
  const openThresholdEditor = (config) => {
    const panel = toolHost.open("thresholdEditor");
    panel.configure(config || createThresholdEditorConfig());
    return panel;
  };
  const clearPreviewAutoRetry = () => {
    if (previewAutoRetryTimer) {
      clearTimeout(previewAutoRetryTimer);
      previewAutoRetryTimer = void 0;
    }
  };
  const cancelPreviewAutoStart = () => {
    clearPreviewAutoRetry();
    previewAutoRetryCount = 0;
    previewAutoStartToken++;
  };
  const waitForPreviewAutoStart = async () => {
    const inFlight = previewAutoStartPromise;
    if (inFlight) {
      await inFlight;
    }
  };
  const clearPreviewWatchdog = () => {
    if (previewWatchdogTimer) {
      clearInterval(previewWatchdogTimer);
      previewWatchdogTimer = void 0;
    }
  };
  const recoverStalePreview = async () => {
    if (previewRecoverInFlight || previewManuallyStopped || previewPausedForScript || !scriptRunning || session.state !== "streaming") {
      return;
    }
    const age = videoService?.lastFrameAgeMs();
    if (age !== null && age !== void 0 && age <= previewFrameStaleMs) {
      return;
    }
    previewRecoverInFlight = true;
    try {
      const ageText = age === null || age === void 0 ? "startup" : `${age}ms`;
      logWarn("Preview", `No frames received for ${ageText}; restarting preview`);
      cancelPreviewAutoStart();
      await stopPreviewRuntime();
      if (!previewManuallyStopped && !previewPausedForScript && scriptRunning) {
        previewAutoRetryCount = 0;
        schedulePreviewAuto(150);
      }
    } catch (err) {
      logDebug("Preview", `Stale preview recovery failed: ${err instanceof Error ? err.message : String(err)}`);
    } finally {
      previewRecoverInFlight = false;
    }
  };
  const updatePreviewWatchdog = () => {
    const shouldWatch = connected && scriptRunning && session.state === "streaming" && !previewManuallyStopped && !previewPausedForScript && !!previewPanel && !previewPanel.disposed;
    if (shouldWatch && !previewWatchdogTimer) {
      previewWatchdogTimer = setInterval(() => {
        void recoverStalePreview();
      }, previewWatchdogIntervalMs);
    } else if (!shouldWatch) {
      clearPreviewWatchdog();
    }
  };
  const schedulePreviewAuto = (delayMs = 0, options = {}) => {
    if (previewManuallyStopped || previewPausedForScript || !scriptRunning || connectionBusy || scriptBusy && !options.allowWhileScriptBusy || session.state !== "connected") {
      return;
    }
    const token = previewAutoStartToken;
    clearPreviewAutoRetry();
    previewAutoRetryTimer = setTimeout(() => {
      previewAutoRetryTimer = void 0;
      const promise = startPreviewAuto(token);
      previewAutoStartPromise = promise;
      void promise.finally(() => {
        if (previewAutoStartPromise === promise) {
          previewAutoStartPromise = void 0;
        }
      });
    }, delayMs);
  };
  const startPreviewAuto = async (token = previewAutoStartToken) => {
    if (token !== previewAutoStartToken || previewManuallyStopped || previewPausedForScript || connectionBusy || scriptBusy || session.state !== "connected") {
      logDebug("Preview", `Auto-start skipped: token=${token === previewAutoStartToken ? "current" : "stale"} manualStop=${previewManuallyStopped} paused=${previewPausedForScript} scriptRun=${scriptRunning} state=${session.state}`);
      return;
    }
    if (!scriptRunning) {
      logDebug("Preview", "Auto-start skipped: no script is running");
      return;
    }
    if (previewAutoStartInFlight) {
      logDebug("Preview", "Auto-start already in flight");
      return;
    }
    previewAutoStartInFlight = true;
    logInfo("Preview", "Auto-starting");
    try {
      ensurePreviewPanel();
      if (token !== previewAutoStartToken || previewPausedForScript || !scriptRunning || connectionBusy || scriptBusy || session.state !== "connected") {
        logDebug("Preview", "Auto-start canceled before request");
        return;
      }
      const started = await getVideoService()?.startPreview(void 0, void 0, { assumeScriptRunning: assumeScriptRunningForPreview(), suppressErrors: true });
      if (token !== previewAutoStartToken || previewPausedForScript || !scriptRunning || connectionBusy || scriptBusy) {
        logDebug("Preview", "Auto-start result discarded after script state changed");
        if (started) {
          await stopPreviewRuntime();
        }
        return;
      }
      if (started) {
        previewAutoRetryCount = 0;
        logInfo("Preview", "Auto-started");
        updatePreviewWatchdog();
        updateVirtualTouchRefreshTimer();
        return;
      }
      if (token === previewAutoStartToken && !previewManuallyStopped && !previewPausedForScript && !connectionBusy && !scriptBusy && scriptRunning && session.state === "connected") {
        const delays = [500, 1e3, 2e3, 3e3, 3e3];
        const delay = delays[Math.min(previewAutoRetryCount, delays.length - 1)];
        previewAutoRetryCount++;
        logDebug("Preview", `Auto-start deferred; retrying in ${delay}ms`);
        schedulePreviewAuto(delay);
      }
    } catch (e) {
      logError("Preview", `Auto-start error: ${e}`);
    } finally {
      previewAutoStartInFlight = false;
    }
  };
  const startPreviewManual = async () => {
    cancelPreviewAutoStart();
    await waitForPreviewAutoStart();
    previewManuallyStopped = false;
    previewPausedForScript = false;
    previewPanel?.sendPreviewDisabled(false);
    if (session.state === "streaming") {
      ensurePreviewPanel();
      return true;
    }
    if (session.state !== "connected") {
      return false;
    }
    ensurePreviewPanel();
    const started = await getVideoService()?.startPreview(void 0, void 0, { assumeScriptRunning: assumeScriptRunningForPreview() });
    if (started) {
      updatePreviewWatchdog();
      updateVirtualTouchRefreshTimer();
    }
    return started === true;
  };
  async function stopPreviewRuntime() {
    if (session.state === "streaming") {
      if (videoService) {
        await videoService.stopPreview();
      } else {
        await session.request(createRequest(Methods.stopPreview, {}));
        session.stopStreaming();
      }
    }
    updatePreviewWatchdog();
    updateVirtualTouchRefreshTimer();
  }
  const stopPreviewManual = async () => {
    cancelPreviewAutoStart();
    previewManuallyStopped = true;
    previewPausedForScript = false;
    await waitForPreviewAutoStart();
    previewPanel?.sendPreviewDisabled(true);
    await stopPreviewRuntime();
    clearVirtualTouchState();
  };
  const setPreviewDisabledManual = async (disabled) => {
    if (disabled) {
      await stopPreviewManual();
    } else {
      await startPreviewManual();
    }
  };
  const stopPreviewBeforeScript = async () => {
    cancelPreviewAutoStart();
    await waitForPreviewAutoStart();
    if (session.state === "streaming") {
      previewPausedForScript = true;
      await stopPreviewRuntime();
    }
  };
  const startPreviewForScript = () => {
    previewAutoStartToken++;
    previewPausedForScript = false;
    previewAutoRetryCount = 0;
    schedulePreviewAuto(1500, { allowWhileScriptBusy: true });
  };
  const showTerminalView = () => {
    void vscode24.commands.executeCommand("canmv.terminalView.focus");
  };
  const showScriptViews = () => {
    toolHost.open("preview");
    showTerminalView();
  };
  const stopPreviewAfterScript = async () => {
    cancelPreviewAutoStart();
    await waitForPreviewAutoStart();
    previewPausedForScript = false;
    await stopPreviewRuntime();
    clearVirtualTouchState();
  };
  const waitForDebugIdle = async (timeoutMs = 6e4) => {
    const deadline = Date.now() + timeoutMs;
    while (Date.now() < deadline && connected) {
      const status = await session.request(createRequest(Methods.scriptRunning, {}), { timeoutMs: 2e3 });
      if (isResponse(status) && status.result.running !== true) {
        setScriptRunningContext(false);
        setBoardReadyContext(true);
        return true;
      }
      await new Promise((resolve) => setTimeout(resolve, 250));
    }
    return false;
  };
  const stopRunningScriptLocked = async (options = {}) => {
    if (!connected) return false;
    scriptStopInFlight = true;
    try {
      cancelPreviewAutoStart();
      if (options.stopPreview) {
        previewPausedForScript = true;
        await waitForPreviewAutoStart();
        await stopPreviewRuntime();
        clearVirtualTouchState();
      }
      resetBoardReadiness();
      const result = await session.request(createRequest(Methods.stopScript, {}));
      if (isResponse(result)) {
        const payload = result.result;
        if (payload.output) {
          appendTerminal(payload.output);
        }
        const stopped = await waitForDebugIdle();
        if (!stopped) {
          const message = "Timed out waiting for Production to release Camera/UART";
          logError("Script", message);
          appendTerminalLine(`[CanMV] ${message}`);
          if (options.notify !== false) vscode24.window.showErrorMessage(`CanMV: ${message}`);
          return false;
        }
        if (options.notify !== false) vscode24.window.showInformationMessage(t("CanMV: Script stopped."));
      } else {
        logError("Script", `Stop failed: ${result.error.message}`);
        appendTerminalLine(`[CanMV] ${result.error.message}`);
        if (options.notify !== false) vscode24.window.showErrorMessage(t("CanMV: Failed to stop script - {message}", { message: result.error.message }));
        return false;
      }
      if (options.stopPreview) {
        await stopPreviewAfterScript();
      }
      return true;
    } finally {
      scriptStopInFlight = false;
    }
  };
  const stopRunningScript = async (options = {}) => {
    if (!connected) return false;
    if (!beginScriptOperation({ allowWhileConnectionBusy: options.allowWhileConnectionBusy, skipCooldown: options.allowWhileConnectionBusy })) return false;
    try {
      return await stopRunningScriptLocked(options);
    } finally {
      endScriptOperation();
    }
  };
  const ensureCanStartScript = async (options = {}) => {
    if (!connected) return false;
    if (!boardHasCapabilitiesProtocol()) {
      if (!boardReady || scriptRunning) {
        vscode24.window.showWarningMessage(t("CanMV: Board is not ready yet. Wait for initialization to finish."));
        return false;
      }
      logDebug("Script", "Skipping scriptRunning precheck: legacy firmware has no capabilities protocol");
      return true;
    }
    const runningResult = await session.request(createRequest(Methods.scriptRunning, {}));
    if (!isResponse(runningResult)) {
      logWarn("Script", `Could not check running state: ${runningResult.error.message}`);
      vscode24.window.showWarningMessage(t("CanMV: Cannot check script state - {message}", { message: runningResult.error.message }));
      return false;
    }
    const running = !!runningResult.result.running;
    if (running) {
      setScriptRunningContext(true);
      appendTerminalLine("[CanMV] Explicit Run takeover: stopping Production and waiting for resource release...");
      const stopped = await stopRunningScriptLocked({ stopPreview: true, notify: options.notify !== false });
      if (!stopped) return false;
    } else {
      setScriptRunningContext(false);
      setBoardReadyContext(true);
    }
    if (!boardReady) {
      vscode24.window.showWarningMessage(t("CanMV: Board is not ready yet. Wait for initialization to finish."));
      return false;
    }
    return true;
  };
  const runRemotePathLocked = async (path15) => {
    if (!await ensureCanStartScript()) return false;
    await stopPreviewBeforeScript();
    logInfo("Script", `Run remote file: ${path15}`);
    try {
      const result = await fileService.fileExec(path15);
      if (result.status !== "started") {
        if (result.message) {
          vscode24.window.showWarningMessage(t("CanMV: {message}", { message: result.message }));
        }
        return false;
      }
      setScriptRunningContext(true);
      startPreviewForScript();
      showScriptViews();
      return true;
    } catch (err) {
      setScriptRunningContext(false);
      throw err;
    }
  };
  const runRemotePath = async (path15) => {
    if (!beginScriptOperation()) return false;
    try {
      return await runRemotePathLocked(path15);
    } finally {
      endScriptOperation();
    }
  };
  const trimRemotePath = (path15) => path15.replace(/\/+$/g, "");
  const childPath = (parentPath, name) => parentPath === "/" ? "/" + name : trimRemotePath(parentPath) + "/" + name;
  const parentRemotePath = (path15) => {
    const trimmed = trimRemotePath(path15);
    const index = trimmed.lastIndexOf("/");
    return index <= 0 ? "/" : trimmed.slice(0, index);
  };
  const remotePathFromCommandArg = (arg) => {
    if (arg instanceof vscode24.Uri && arg.scheme === "canmv") {
      return arg.path;
    }
    if (arg instanceof vscode24.Uri && arg.scheme === "file") {
      return remoteMirrorService.remotePathForUri(arg);
    }
    if (arg instanceof FileTreeItem) {
      return arg.absPath;
    }
    return selectedExplorerItem()?.absPath;
  };
  const showRemoteOperationError = (operation, err) => {
    const message = err instanceof Error ? err.message : String(err);
    void vscode24.window.showErrorMessage(t("CanMV: {operation} failed - {message}", { operation, message }));
  };
  let thresholdSelection;
  const parseThresholdTuple = (text) => {
    const trimmed = text.trim();
    const match = /^\(\s*([+-]?\d+)\s*,\s*([+-]?\d+)(?:\s*,\s*([+-]?\d+)\s*,\s*([+-]?\d+)\s*,\s*([+-]?\d+)\s*,\s*([+-]?\d+))?\s*\)$/.exec(trimmed);
    if (!match) return void 0;
    const values = match.slice(1).filter((value) => value !== void 0).map((value) => Number.parseInt(value, 10));
    if (values.some((value) => !Number.isFinite(value))) return void 0;
    if (values.length === 2) return { mode: "grayscale", values };
    if (values.length === 6) return { mode: "lab", values };
    return void 0;
  };
  const thresholdSelectionFromEditor = () => {
    const editor = vscode24.window.activeTextEditor;
    if (!editor || editor.selection.isEmpty) return void 0;
    const parsed = parseThresholdTuple(editor.document.getText(editor.selection));
    if (!parsed) return void 0;
    return {
      ...parsed,
      range: editor.selection,
      uri: editor.document.uri
    };
  };
  const createThresholdEditorConfig = () => {
    thresholdSelection = thresholdSelectionFromEditor();
    if (!thresholdSelection) {
      return { canApplyToEditor: false };
    }
    return {
      mode: thresholdSelection.mode,
      values: thresholdSelection.values,
      canApplyToEditor: true
    };
  };
  const applyThresholdToSelection = async (text) => {
    if (!thresholdSelection) {
      vscode24.window.showWarningMessage(t("CanMV: Select a grayscale or LAB threshold tuple before applying."));
      return;
    }
    const document = await vscode24.workspace.openTextDocument(thresholdSelection.uri);
    const editor = await vscode24.window.showTextDocument(document, { preview: false });
    await editor.edit((builder) => {
      builder.replace(thresholdSelection.range, text);
    });
    thresholdSelection = {
      ...thresholdSelection,
      values: parseThresholdTuple(text)?.values || thresholdSelection.values,
      range: new vscode24.Range(thresholdSelection.range.start, thresholdSelection.range.start.translate(0, text.length))
    };
    thresholdEditorPanel?.sendApplied();
  };
  const promptRemoteName = async (prompt, value = "") => vscode24.window.showInputBox({
    prompt,
    value,
    validateInput: (input) => {
      const name = input.trim();
      if (!name) return t("Name is required");
      if (name.includes("/")) return t("Use a name, not a path");
      return void 0;
    }
  });
  const ensureRemoteFilesAvailable = () => {
    if (remoteFilesAvailable()) return true;
    vscode24.window.showWarningMessage(t("CanMV: {message}", { message: remoteFilesUnavailableMessage() }));
    return false;
  };
  const refreshExplorer = () => {
    if (!explorerCanBrowse()) {
      updateExplorerConnectionState();
      return;
    }
    fileService.clearCache();
    explorer?.refresh();
  };
  refreshExplorerSoon = (delayMs = 250) => {
    if (explorerRefreshTimer) {
      clearTimeout(explorerRefreshTimer);
    }
    explorerRefreshTimer = setTimeout(() => {
      explorerRefreshTimer = void 0;
      refreshExplorer();
    }, delayMs);
  };
  const fsProvider = new CanmvFileSystemProvider(fileService, {
    isAvailable: () => remoteFilesAvailable(),
    unavailableMessage: () => remoteFilesUnavailableMessage()
  });
  context.subscriptions.push(
    vscode24.workspace.registerFileSystemProvider("canmv", fsProvider)
  );
  const canmvExplorer = new CanmvExplorer({
    listDir: async (path15) => {
      if (!explorerCanBrowse()) {
        return [];
      }
      return fileService.listDir(path15);
    },
    listDirPage: async (path15, offset) => {
      if (!explorerCanBrowse()) {
        return { entries: [] };
      }
      return fileService.listDirPage(path15, offset);
    }
  });
  explorer = canmvExplorer;
  const treeView = vscode24.window.createTreeView("canmv.explorer", {
    treeDataProvider: canmvExplorer,
    showCollapseAll: true
  });
  context.subscriptions.push(treeView);
  const selectedExplorerItem = () => treeView.selection.length === 1 ? treeView.selection[0] : void 0;
  const updateExplorerSelectionContexts = () => {
    const item = selectedExplorerItem();
    const hasSelection = !!item?.absPath;
    void vscode24.commands.executeCommand("setContext", "canmv.explorerSelected", hasSelection);
    void vscode24.commands.executeCommand("setContext", "canmv.explorerSelectedDirectory", hasSelection && item?.fileType === "directory");
    void vscode24.commands.executeCommand("setContext", "canmv.explorerSelectedMutable", hasSelection && item?.contextValue !== "mountRoot");
  };
  updateExplorerSelectionContexts();
  context.subscriptions.push(treeView.onDidChangeSelection(updateExplorerSelectionContexts));
  context.subscriptions.push(treeView.onDidExpandElement((event) => canmvExplorer.resumeListing(event.element)));
  controlProvider = new CanmvControlViewProvider(context);
  context.subscriptions.push(
    vscode24.window.registerWebviewViewProvider("canmv.controls", controlProvider, {
      webviewOptions: { retainContextWhenHidden: true }
    })
  );
  controlProvider.setState({
    connected,
    scriptRunning,
    boardReady,
    connectionPhase,
    statusText: sidebarStatusText(session.state)
  });
  const toolboxProvider = new ToolboxTreeProvider(registry);
  const toolboxView = vscode24.window.createTreeView("canmv.toolbox", {
    treeDataProvider: toolboxProvider
  });
  context.subscriptions.push(toolboxView);
  terminalViewProvider = new TerminalViewProvider(context, () => terminalScrollback.join(""));
  terminalViewProvider.onClear(() => {
    terminalScrollback.length = 0;
    terminalScrollbackSize = 0;
  });
  let terminalInputQueue = Promise.resolve();
  terminalViewProvider.onInput((text) => {
    const isCtrlC = text === "";
    if (scriptRunning && isCtrlC) {
      logInfo("Terminal", "Ctrl-C requested script stop");
      terminalInputQueue = Promise.resolve();
      void stopRunningScript({ stopPreview: true }).catch((err) => {
        logError("Terminal", `Ctrl-C stop error: ${err instanceof Error ? err.message : String(err)}`);
      });
      return;
    }
    const terminalCanSend = session.state === "connected" || session.state === "streaming";
    if (!terminalCanSend || !boardReady || connectionBusy || scriptBusy || scriptRunning) {
      updateTerminalInputState();
      return;
    }
    if (!boardSupportsReplInput()) {
      updateTerminalInputState();
      return;
    }
    const req = createRequest(Methods.terminalInput, { text });
    const activeBackend = backend;
    if (activeBackend?.notify) {
      activeBackend.notify(req);
      return;
    }
    terminalInputQueue = terminalInputQueue.then(async () => {
      const result = await session.request(req);
      if (!isResponse(result)) {
        logWarn("Terminal", `Input error: ${result.error.message}`);
      }
    }).catch((err) => {
      logError("Terminal", `Input queue error: ${err instanceof Error ? err.message : String(err)}`);
    });
  });
  context.subscriptions.push(
    vscode24.window.registerWebviewViewProvider("canmv.terminalView", terminalViewProvider, {
      webviewOptions: { retainContextWhenHidden: true }
    })
  );
  updateTerminalInputState();
  const connectBoardRuntime = async (options = {}) => {
    if (connected) return boardService.boardInfo();
    if (!disconnected || connectionBusy || scriptBusy) return null;
    setConnectionPhase("connecting");
    setConnectionBusyContext(true);
    cancelPreviewAutoStart();
    resetBoardReadiness();
    try {
      fileService.clearCache();
      const repl = await boardService.connectBoard(options);
      const info = boardService.boardInfo();
      if (info) {
        if (!await syncBoardExecutionState()) {
          setScriptRunningContext(false);
          setBoardReadyContext(pendingBoardReadyEvent || !boardHasCapabilitiesProtocol());
        }
        updateExplorerConnectionState();
        updateBoardStatus();
        previewPanel?.sendBoardInfo(info);
      } else {
        setBoardReadyContext(false);
      }
      if (repl) appendTerminal(repl);
      updateTerminalInputState();
      mcpBridge?.broadcastSnapshot();
      return info;
    } finally {
      setConnectionBusyContext(false);
      setConnectionPhase("idle");
      // A connected Production script may already be running. Connecting is
      // observational, but Preview should attach to its VIRT framebuffer.
      if (connected && scriptRunning) schedulePreviewAuto(150);
    }
  };
  const disconnectBoardRuntime = async () => {
    if (!connected || connectionBusy || scriptBusy) return;
    setConnectionPhase("disconnecting");
    setConnectionBusyContext(true);
    try {
      cancelPreviewAutoStart();
      previewPausedForScript = false;
      clearVirtualTouchState();
      updateVirtualTouchRefreshTimer();
      // Disconnect is transport-only. A running Production or Debug script
      // continues on the board and retains its Camera/UART ownership.
      videoService?.clearPreviewState();
      resetBoardReadiness();
      fileService.clearCache();
      await boardService.disconnectBoard();
      setStatusForState("disconnected");
      updateTerminalInputState();
      appendTerminalLine(t("[CanMV] Disconnected"));
      mcpBridge?.broadcastSnapshot();
    } finally {
      setConnectionBusyContext(false);
      setConnectionPhase("idle");
    }
  };
  disposables = [
    vscode24.commands.registerCommand("canmv.connectBoard", async () => {
      await connectBoardRuntime();
    }),
    vscode24.commands.registerCommand("canmv.disconnectBoard", async () => {
      await disconnectBoardRuntime();
    }),
    vscode24.commands.registerCommand("canmv.runCurrentScript", async () => {
      if (!beginScriptOperation()) return;
      let started = false;
      try {
        if (!await ensureCanStartScript()) return;
        await stopPreviewBeforeScript();
        started = await scriptService.runCurrentScript();
        if (!started) {
          setScriptRunningContext(false);
        } else {
          setScriptRunningContext(true);
          startPreviewForScript();
          showScriptViews();
        }
      } catch (err) {
        if (!started) {
          setScriptRunningContext(false);
        }
        throw err;
      } finally {
        endScriptOperation();
      }
    }),
    vscode24.commands.registerCommand("canmv.stopScript", async () => {
      if (!connected || !scriptRunning || scriptBusy || connectionBusy) return;
      await stopRunningScript({ stopPreview: true });
    }),
    vscode24.commands.registerCommand("canmv.startPreview", async () => {
      await startPreviewManual();
    }),
    vscode24.commands.registerCommand("canmv.stopPreview", async () => {
      await stopPreviewManual();
    }),
    vscode24.commands.registerCommand("canmv.runRemoteFile", async (arg) => {
      const path15 = remotePathFromCommandArg(arg);
      if (!path15 || !path15.endsWith(".py")) return;
      await runRemotePath(path15);
    }),
    vscode24.commands.registerCommand("canmv.runExampleFile", async (item) => {
      const fsPath = item instanceof vscode24.Uri ? item.fsPath : item?.fsPath;
      if (!fsPath || !fsPath.toLowerCase().endsWith(".py")) return;
      if (!beginScriptOperation()) return;
      let started = false;
      try {
        if (!await ensureCanStartScript()) return;
        await stopPreviewBeforeScript();
        const script = fs13.readFileSync(fsPath, "utf8");
        started = await scriptService.runScriptContent(script, path14.basename(fsPath));
        if (!started) {
          setScriptRunningContext(false);
        } else {
          setScriptRunningContext(true);
          startPreviewForScript();
          showScriptViews();
        }
      } catch (err) {
        if (!started) {
          setScriptRunningContext(false);
        }
        logError("Script", `Run example failed: ${err}`);
        vscode24.window.showErrorMessage(t("CanMV: {message}", { message: err instanceof Error ? err.message : String(err) }));
      } finally {
        endScriptOperation();
      }
    }),
    vscode24.commands.registerCommand("canmv.openRemoteFile", async (arg) => {
      const path15 = remotePathFromCommandArg(arg);
      if (!path15) return;
      if (!ensureRemoteFilesAvailable()) return;
      try {
        await remoteMirrorService.openRemoteFile(path15);
      } catch (err) {
        showRemoteOperationError("Open remote file", err);
      }
    }),
    vscode24.commands.registerCommand("canmv.runOnK230", async () => {
      if (!beginScriptOperation()) return;
      let started = false;
      try {
        if (!await ensureCanStartScript()) return;
        const editor = vscode24.window.activeTextEditor;
        if (!editor) return;
        const uri = editor.document.uri;
        const mirroredRemotePath = remoteMirrorService.remotePathForDocument(editor.document);
        if (uri.scheme === "canmv") {
          if (editor.document.isDirty) await editor.document.save();
          started = await runRemotePathLocked(uri.path);
          return;
        } else if (mirroredRemotePath) {
          if (editor.document.isDirty) await editor.document.save();
          await remoteMirrorService.syncDocumentToRemote(editor.document);
          started = await runRemotePathLocked(mirroredRemotePath);
          return;
        } else {
          await stopPreviewBeforeScript();
          const script = editor.document.getText();
          logInfo("Script", `Run active file on K230: ${uri.fsPath} (${script.length}B)`);
          const req = createRequest(Methods.runScript, { script });
          const result = await session.request(req);
          if (!isResponse(result)) {
            vscode24.window.showErrorMessage(t("CanMV: {message}", { message: result.error.message }));
          } else if (result.result.status !== "ok") {
            const payload = result.result;
            vscode24.window.showWarningMessage(t("CanMV: {message}", { message: payload.message || payload.output || t("Script did not start") }));
          } else {
            started = true;
            setScriptRunningContext(true);
            startPreviewForScript();
            showScriptViews();
          }
        }
      } catch (err) {
        if (!started) {
          setScriptRunningContext(false);
        }
        throw err;
      } finally {
        endScriptOperation();
      }
    }),
    vscode24.commands.registerCommand("canmv.saveAsMainPy", async () => {
      const editor = vscode24.window.activeTextEditor;
      if (!editor) return;
      if (!ensureRemoteFilesAvailable()) return;
      const text = editor.document.getText();
      const data = new TextEncoder().encode(text);
      try {
        const ok = await fileService.writeFile("/sdcard/main.py", data);
        if (!ok) {
          vscode24.window.showWarningMessage(t("CanMV: Save as /sdcard/main.py was rejected by the board"));
          return;
        }
        vscode24.window.showInformationMessage(t("CanMV: Saved as /sdcard/main.py"));
      } catch (err) {
        showRemoteOperationError(t("Save as /sdcard/main.py"), err);
      }
    }),
    vscode24.commands.registerCommand("canmv.saveAsBootPy", async () => {
      const editor = vscode24.window.activeTextEditor;
      if (!editor) return;
      if (!ensureRemoteFilesAvailable()) return;
      const text = editor.document.getText();
      const data = new TextEncoder().encode(text);
      try {
        const ok = await fileService.writeFile("/sdcard/boot.py", data);
        if (!ok) {
          vscode24.window.showWarningMessage(t("CanMV: Save as /sdcard/boot.py was rejected by the board"));
          return;
        }
        vscode24.window.showInformationMessage(t("CanMV: Saved as /sdcard/boot.py"));
      } catch (err) {
        showRemoteOperationError(t("Save as /sdcard/boot.py"), err);
      }
    }),
    vscode24.commands.registerCommand("canmv.openTool", (toolId) => {
      if (toolId) {
        toolHost.open(toolId);
      } else {
        const items = registry.listVisible().map((t2) => ({ label: t2.name, id: t2.id }));
        vscode24.window.showQuickPick(items).then((pick) => {
          if (pick) toolHost.open(pick.id);
        });
      }
    }),
    vscode24.commands.registerCommand("canmv.openThresholdEditor", () => {
      openThresholdEditor(createThresholdEditorConfig());
    }),
    vscode24.commands.registerCommand("canmv.newRemoteFile", async (item) => {
      item = item ?? selectedExplorerItem();
      if (!connected || !item || item.fileType !== "directory") return;
      if (!ensureRemoteFilesAvailable()) return;
      const name = await promptRemoteName(t("New file name"));
      if (!name) return;
      const path15 = childPath(item.absPath, name.trim());
      try {
        const ok = await fileService.writeFile(path15, new Uint8Array());
        if (!ok) throw new Error(t("backend rejected the request"));
        refreshExplorer();
        await remoteMirrorService.openRemoteFile(path15);
      } catch (err) {
        showRemoteOperationError(t("Create file"), err);
      }
    }),
    vscode24.commands.registerCommand("canmv.newRemoteFolder", async (item) => {
      item = item ?? selectedExplorerItem();
      if (!connected || !item || item.fileType !== "directory") return;
      if (!ensureRemoteFilesAvailable()) return;
      const name = await promptRemoteName(t("New folder name"));
      if (!name) return;
      const path15 = childPath(item.absPath, name.trim());
      try {
        const ok = await fileService.mkdir(path15);
        if (!ok) throw new Error(t("backend rejected the request"));
        refreshExplorer();
      } catch (err) {
        showRemoteOperationError(t("Create folder"), err);
      }
    }),
    vscode24.commands.registerCommand("canmv.uploadFiles", async (item) => {
      item = item ?? selectedExplorerItem();
      if (!connected || !item || item.fileType !== "directory") return;
      if (!ensureRemoteFilesAvailable()) return;
      const files = await vscode24.window.showOpenDialog({
        canSelectFiles: true,
        canSelectFolders: false,
        canSelectMany: true,
        openLabel: t("Upload Files")
      });
      if (!files || files.length === 0) return;
      const uploads = files.map((file) => {
        const remotePath = childPath(item.absPath, path14.basename(file.fsPath));
        return { file, remotePath, totals: fileService.measureUpload(file.fsPath, remotePath) };
      });
      const totalBytes = uploads.reduce((sum, upload) => sum + upload.totals.bytes, 0);
      const totalFiles = uploads.reduce((sum, upload) => sum + upload.totals.files, 0);
      try {
        await vscode24.window.withProgress(
          { location: vscode24.ProgressLocation.Notification, title: t("Uploading files to CanMV") },
          async (progress) => {
            const tracker = { lastBytes: 0 };
            let byteOffset = 0;
            let fileOffset = 0;
            for (const upload of uploads) {
              await fileService.upload(upload.file.fsPath, upload.remotePath, (event) => {
                reportFileTransferProgress(progress, event, tracker, {
                  byteOffset,
                  fileOffset,
                  totalBytes,
                  totalFiles
                });
              });
              byteOffset += upload.totals.bytes;
              fileOffset += upload.totals.files;
            }
          }
        );
        refreshExplorer();
      } catch (err) {
        showRemoteOperationError(t("Upload files"), err);
      }
    }),
    vscode24.commands.registerCommand("canmv.uploadFolder", async (item) => {
      item = item ?? selectedExplorerItem();
      if (!connected || !item || item.fileType !== "directory") return;
      if (!ensureRemoteFilesAvailable()) return;
      const folders = await vscode24.window.showOpenDialog({
        canSelectFiles: false,
        canSelectFolders: true,
        canSelectMany: false,
        openLabel: t("Upload Folder")
      });
      if (!folders || folders.length === 0) return;
      const folder = folders[0];
      const remotePath = childPath(item.absPath, path14.basename(folder.fsPath));
      const totals = fileService.measureUpload(folder.fsPath, remotePath);
      try {
        await vscode24.window.withProgress(
          { location: vscode24.ProgressLocation.Notification, title: t("Uploading folder to CanMV") },
          async (progress) => {
            const tracker = { lastBytes: 0 };
            await fileService.upload(folder.fsPath, remotePath, (event) => {
              reportFileTransferProgress(progress, event, tracker, {
                totalBytes: totals.bytes,
                totalFiles: totals.files
              });
            });
          }
        );
        refreshExplorer();
      } catch (err) {
        showRemoteOperationError(t("Upload folder"), err);
      }
    }),
    vscode24.commands.registerCommand("canmv.downloadRemoteItem", async (item) => {
      item = item ?? selectedExplorerItem();
      if (!connected || !item || !item.absPath) return;
      if (!ensureRemoteFilesAvailable()) return;
      const folders = await vscode24.window.showOpenDialog({
        canSelectFiles: false,
        canSelectFolders: true,
        canSelectMany: false,
        openLabel: t("Download Here"),
        title: t("Select Download Folder")
      });
      if (!folders || folders.length === 0) return;
      const localPath = path14.join(folders[0].fsPath, item.name || path14.basename(item.absPath));
      const localUri = vscode24.Uri.file(localPath);
      let targetExists = false;
      try {
        await vscode24.workspace.fs.stat(localUri);
        targetExists = true;
      } catch {
        targetExists = false;
      }
      if (targetExists) {
        const action = item.fileType === "directory" ? t("Merge and Overwrite") : t("Overwrite");
        const confirmed = await vscode24.window.showWarningMessage(
          t('"{name}" already exists in the selected folder.', { name: path14.basename(localPath) }),
          { modal: true, detail: item.fileType === "directory" ? t("Existing files with matching names may be overwritten.") : t("The existing local file will be overwritten.") },
          action
        );
        if (confirmed !== action) return;
      }
      const label = item.fileType === "directory" ? t("folder") : t("file");
      try {
        await vscode24.window.withProgress(
          { location: vscode24.ProgressLocation.Notification, title: t("Downloading {label} from CanMV", { label }) },
          async (progress) => {
            const tracker = { lastBytes: 0 };
            await fileService.download(item.absPath, localPath, (event) => {
              reportFileTransferProgress(progress, event, tracker);
            });
          }
        );
        void vscode24.window.showInformationMessage(t("CanMV: Downloaded {name} to {path}", { name: item.name, path: localPath }));
      } catch (err) {
        showRemoteOperationError(t("Download"), err);
      }
    }),
    vscode24.commands.registerCommand("canmv.renameRemoteItem", async (item) => {
      item = item ?? selectedExplorerItem();
      if (!connected || !item || item.contextValue === "mountRoot") return;
      if (!ensureRemoteFilesAvailable()) return;
      const name = await promptRemoteName(t("New name"), item.name || "");
      if (!name || name.trim() === item.name) return;
      const parent = parentRemotePath(item.absPath);
      const newPath = childPath(parent, name.trim());
      try {
        const ok = await fileService.renameFile(item.absPath, newPath);
        if (!ok) throw new Error(t("backend rejected the request"));
        refreshExplorer();
      } catch (err) {
        showRemoteOperationError(t("Rename"), err);
      }
    }),
    vscode24.commands.registerCommand("canmv.deleteRemoteItem", async (item) => {
      item = item ?? selectedExplorerItem();
      if (!connected || !item || item.contextValue === "mountRoot") return;
      if (!ensureRemoteFilesAvailable()) return;
      const label = item.fileType === "directory" ? t("folder") : t("file");
      const deleteAction = t("Delete");
      const confirmed = await vscode24.window.showWarningMessage(
        t('Delete {label} "{name}" from CanMV?', { label, name: item.name }),
        { modal: true },
        deleteAction
      );
      if (confirmed !== deleteAction) return;
      try {
        const ok = item.fileType === "directory" ? await fileService.rmdir(item.absPath, true) : await fileService.deleteFile(item.absPath);
        if (!ok) throw new Error(t("backend rejected the request"));
        refreshExplorer();
      } catch (err) {
        showRemoteOperationError(t("Delete"), err);
      }
    }),
    vscode24.commands.registerCommand("canmv.refreshExplorer", () => {
      refreshExplorer();
    }),
    vscode24.commands.registerCommand("canmv.refreshExamples", async () => {
      try {
        await canmvResourceService?.ensureDefaultExamples();
      } catch (err) {
        logError("Examples", `Refresh examples failed: ${err}`);
      }
      examplesService?.refresh();
    }),
    vscode24.commands.registerCommand("canmv.openExampleFile", async (item) => {
      const fsPath = item instanceof vscode24.Uri ? item.fsPath : item?.fsPath;
      if (!fsPath) return;
      const content = fs13.readFileSync(fsPath, "utf8");
      const doc = await vscode24.workspace.openTextDocument({
        content,
        language: languageForExampleFile(fsPath)
      });
      await vscode24.window.showTextDocument(doc, { preview: true });
    }),
    vscode24.commands.registerCommand("canmv.revealExamples", async (item) => {
      const target = item?.fsPath || examplesService?.activeExamplesDir() || examplesService?.examplesRootDir();
      if (!target) return;
      await vscode24.commands.executeCommand("revealFileInOS", vscode24.Uri.file(target));
    })
  ];
  context.subscriptions.push(...disposables);
  context.subscriptions.push(vscode24.workspace.onDidSaveTextDocument((document) => {
    void remoteMirrorService.syncDocumentToRemote(document).catch((err) => {
      showRemoteOperationError(t("Sync remote file"), err);
    });
  }));
  backend.onEvent((event) => {
    mcpBridge?.broadcastEvent(event);
    if (event.event === "scriptOutput") {
      const text = event.params.text || "";
      appendTerminal(text);
    } else if (event.event === "scriptState") {
      const s = event.params.state;
      if (s === "started") {
        setScriptRunningContext(true);
      }
      if (s === "finished") {
        setScriptRunningContext(false);
        void stopPreviewAfterScript();
      }
    } else if (event.event === "boardReady") {
      markBoardReadyEvent();
      if (boardReady) {
        logInfo("Board", "Ready after connect soft reboot");
        void configureBoardStubs(session);
        refreshExplorerSoon(300);
        schedulePreviewAuto(150);
      }
    } else if (event.event === "boardDisconnected") {
      const params = event.params;
      const detail = [params.source, params.message].filter(Boolean).join(": ");
      logWarn("Board", `Disconnected${detail ? ` (${detail})` : ""}`);
      cancelPreviewAutoStart();
      previewPausedForScript = false;
      resetBoardReadiness();
      setScriptRunningContext(false);
      clearVirtualTouchState();
      updatePreviewWatchdog();
      updateVirtualTouchRefreshTimer();
      videoService?.clearPreviewState();
      if (connected) {
        void session.disconnect().catch((err) => {
          logWarn("Session", `Disconnect after board loss failed: ${err instanceof Error ? err.message : String(err)}`);
        });
      }
    }
  });
  session.onStateChange((state) => {
    previewPanel?.sendState(state);
    const nextConnected = state === "connected" || state === "streaming";
    setConnectionContexts(state);
    updateExplorerConnectionState();
    updatePreviewWatchdog();
    updateVirtualTouchRefreshTimer();
    if (state !== "streaming") {
      clearVirtualTouchState();
    }
    if (!nextConnected) {
      cancelPreviewAutoStart();
      resetBoardReadiness();
      fileService.clearCache();
      setScriptRunningContext(false);
      previewPausedForScript = false;
      clearVirtualTouchState();
      updateVirtualTouchRefreshTimer();
      videoService?.clearPreviewState();
    }
    if (nextConnected) {
      updateBoardStatus();
      if (state === "connected" && boardService.boardInfo() && !connectionBusy) {
        void syncBoardExecutionState();
      }
    } else {
      setStatusForState(state);
    }
    mcpBridge?.broadcastSnapshot();
  });
  const sendBridgeRequest = async (method, params) => {
    const request3 = createRequest({
      method,
      params: {},
      result: {},
      errors: {}
    }, params);
    if (method === Methods.detectBoards.method && session.state === "disconnected") {
      const baudRate = vscode24.workspace.getConfiguration("canmv").get("baudRate", 12e6);
      try {
        await backend.open("__detect__", baudRate);
        return await session.request(request3);
      } finally {
        await backend.close();
      }
    }
    if (method === Methods.connectBoard.method) {
      const requestedPort = typeof params.port === "string" ? params.port : void 0;
      const current = boardService.boardInfo();
      if (connected && current) {
        if (requestedPort && current.port && requestedPort !== current.port) {
          return {
            id: request3.id,
            error: { code: 1003, message: `Already connected to ${current.port}` }
          };
        }
        return {
          id: request3.id,
          result: {
            ...current,
            repl: "",
            mcpBridgeReady: boardReady,
            mcpBridgeScriptRunning: scriptRunning
          }
        };
      }
      const info = await connectBoardRuntime({
        port: requestedPort,
        baudRate: typeof params.baudRate === "number" ? params.baudRate : void 0,
        interactive: false,
        notify: false
      });
      if (!info) {
        return { id: request3.id, error: { code: 1001, message: "Unable to connect to the CanMV board" } };
      }
      return {
        id: request3.id,
        result: {
          ...info,
          mcpBridgeReady: boardReady,
          mcpBridgeScriptRunning: scriptRunning
        }
      };
    }
    if (method === Methods.disconnectBoard.method) {
      if (connectionBusy || scriptBusy) {
        return { id: request3.id, error: { code: 1003, message: "Another CanMV operation is in progress" } };
      }
      if (connected) await disconnectBoardRuntime();
      return { id: request3.id, result: {} };
    }
    if (!connected) {
      return { id: request3.id, error: { code: 1004, message: "Not connected" } };
    }
    if (method === Methods.startPreview.method) {
      setScriptBusyContext(true);
      try {
        const started = await startPreviewManual();
        if (!started) {
          return { id: request3.id, error: { code: 3002, message: "Unable to start preview" } };
        }
        return { id: request3.id, result: { streamId: "default" } };
      } finally {
        setScriptBusyContext(false);
      }
    }
    if (method === Methods.stopPreview.method) {
      setScriptBusyContext(true);
      try {
        await stopPreviewManual();
        return { id: request3.id, result: {} };
      } finally {
        setScriptBusyContext(false);
      }
    }
    setScriptBusyContext(true);
    try {
      if (method === Methods.runScript.method || method === Methods.ioFileExec.method) {
        if (!await ensureCanStartScript({ notify: false })) {
          return { id: request3.id, error: { code: 2002, message: "Unable to enter DEBUG_IDLE" } };
        }
        await stopPreviewBeforeScript();
      }
      const response = await session.request(request3);
      if (!isResponse(response)) return response;
      if (method === Methods.runScript.method || method === Methods.ioFileExec.method) {
        const status = response.result.status;
        if (status === "ok" || status === "started") {
          setScriptRunningContext(true);
          startPreviewForScript();
          showScriptViews();
        }
      } else if (method === Methods.stopScript.method) {
        setScriptRunningContext(false);
      } else if (method === Methods.scriptRunning.method) {
        setScriptRunningContext(response.result.running === true);
      }
      if (method.startsWith("io.")) refreshExplorerSoon(200);
      return response;
    } finally {
      setScriptBusyContext(false);
    }
  };
  mcpBridge = new McpBridgeServer(context, sendBridgeRequest, () => {
    const info = boardService.boardInfo();
    return {
      board: connected && info ? { ...info, repl: void 0 } : void 0,
      boardReady,
      scriptRunning,
      streaming: session.state === "streaming"
    };
  });
  context.subscriptions.push(mcpBridge);
  try {
    const bridgeInfo = await mcpBridge.start();
    await registerMcpSupport(context, bridgeInfo);
  } catch (err) {
    logWarn("MCP", `Local bridge unavailable: ${err instanceof Error ? err.message : String(err)}`);
    await registerMcpSupport(context);
  }
  logInfo("Extension", "Activation complete");
}
function reportFileTransferProgress(progress, event, tracker, scope = {}) {
  const totalBytes = scope.totalBytes ?? event.totalBytes;
  const totalFiles = scope.totalFiles ?? event.totalFiles;
  const bytesTransferred = Math.min(totalBytes, (scope.byteOffset ?? 0) + event.bytesTransferred);
  const filesTransferred = Math.min(totalFiles, (scope.fileOffset ?? 0) + event.filesTransferred);
  const increment = totalBytes > 0 ? Math.max(0, (bytesTransferred - tracker.lastBytes) * 100 / totalBytes) : void 0;
  tracker.lastBytes = Math.max(tracker.lastBytes, bytesTransferred);
  const name = path14.basename(event.path) || event.path;
  let message;
  if (event.phase === "scanning") {
    message = t("Scanning {name}", { name });
  } else if (event.phase === "hashing") {
    message = t("Hashing {name}", { name });
  } else if (event.phase === "verifying") {
    message = t("Verifying {name}", { name });
  } else if (totalBytes > 0) {
    const percent = Math.min(100, Math.floor(bytesTransferred * 100 / totalBytes));
    message = `${name} - ${formatTransferSize(bytesTransferred)} / ${formatTransferSize(totalBytes)} (${percent}%)`;
  } else {
    message = name;
  }
  if (totalFiles > 1) {
    message += ` - ${filesTransferred}/${totalFiles} ${t("files")}`;
  }
  progress.report({ message, increment });
}
function formatTransferSize(size) {
  if (!Number.isFinite(size) || size <= 0) return "0 B";
  const units = ["B", "KiB", "MiB", "GiB"];
  let value = size;
  let unit = units[0];
  for (let index = 1; index < units.length && value >= 1024; index++) {
    value /= 1024;
    unit = units[index];
  }
  return `${value.toFixed(unit === "B" || value >= 10 ? 0 : 1)} ${unit}`;
}
function logActivationInfo(context) {
  const pkg = context.extension.packageJSON;
  const buildInfo = readBuildInfo(context);
  const extensionId = context.extension.id || [pkg.publisher, pkg.name].filter(Boolean).join(".");
  const mode = vscode24.ExtensionMode[context.extensionMode] || String(context.extensionMode);
  const version2 = pkg.version || "unknown";
  const displayName = pkg.displayName || pkg.name || extensionId || "CanMV";
  const commitId = shortCommit(buildInfo.commit) || buildInfo.shortCommit || readGitCommit(context.extensionPath) || "unknown";
  logInfo("Extension", `Activated ${displayName} ${version2}`);
  logInfo("Extension", `ID: ${extensionId || "unknown"}`);
  logInfo("Extension", `Commit: ${commitId}${buildInfo.dirty ? "-dirty" : ""}`);
  if (buildInfo.builtAt) {
    logInfo("Extension", `Built: ${buildInfo.builtAt}`);
  }
  logInfo("Extension", `Mode: ${mode}`);
  logInfo("Extension", `VS Code: ${vscode24.version}`);
  logInfo("Extension", `Runtime: ${process.platform}-${process.arch}, Node ${process.versions.node}, Electron ${process.versions.electron || "n/a"}`);
  logInfo("Extension", `Path: ${context.extensionPath}`);
}
function readBuildInfo(context) {
  const file = path14.join(context.extensionPath, "build-info.json");
  try {
    return JSON.parse(fs13.readFileSync(file, "utf8"));
  } catch {
    return {};
  }
}
function readGitCommit(extensionPath) {
  try {
    const output = cp5.execFileSync("git", ["-C", extensionPath, "rev-parse", "--short=12", "HEAD"], {
      encoding: "utf8",
      timeout: 1e3,
      stdio: ["ignore", "pipe", "ignore"]
    });
    return output.trim();
  } catch {
    return "";
  }
}
function shortCommit(commit) {
  return commit ? commit.slice(0, 12) : "";
}
function languageForExampleFile(filePath) {
  const ext = path14.extname(filePath).toLowerCase();
  switch (ext) {
    case ".py":
      return "python";
    case ".json":
      return "json";
    case ".md":
      return "markdown";
    case ".yml":
    case ".yaml":
      return "yaml";
    case ".sh":
      return "shellscript";
    case ".c":
    case ".h":
      return "c";
    case ".cpp":
    case ".hpp":
      return "cpp";
    default:
      return "plaintext";
  }
}
async function deactivate() {
  if (backend) {
    backend.disposeSync();
  }
  disposables.forEach((d) => d.dispose());
  logInfo("Extension", "Deactivated");
}
async function fetchCommitFromBoard(session) {
  try {
    const req = createRequest(Methods.getFirmwareCommit, {});
    const result = await session.request(req);
    if (isResponse(result)) {
      const { commitId } = result.result;
      logInfo("Stubs", `Board firmware revision ${commitId ? "detected" : "not available"}`);
      return commitId || "";
    }
  } catch (err) {
    logWarn("Stubs", `getFirmwareCommit error: ${err}`);
  }
  return "";
}
async function configureBoardStubs(session) {
  const commitId = await fetchCommitFromBoard(session);
  await configureResources(canmvResourceService, commitId);
}
async function configureResources(svc, commitId) {
  if (!commitId) {
    logInfo("Stubs", "No board revision available; using default stubs");
  }
  try {
    if (commitId) {
      await svc.ensureBoardResources(commitId);
    } else {
      await svc.ensureDefaultResources();
    }
  } catch (err) {
    logError("Stubs", `Setup error: ${err}`);
  }
}
// Annotate the CommonJS export names for ESM import in node:
0 && (module.exports = {
  activate,
  deactivate
});
/*! Bundled license information:

smol-toml/dist/error.js:
smol-toml/dist/util.js:
smol-toml/dist/date.js:
smol-toml/dist/primitive.js:
smol-toml/dist/extract.js:
smol-toml/dist/struct.js:
smol-toml/dist/parse.js:
smol-toml/dist/stringify.js:
smol-toml/dist/index.js:
  (*!
   * Copyright (c) Squirrel Chat et al., All rights reserved.
   * SPDX-License-Identifier: BSD-3-Clause
   *
   * Redistribution and use in source and binary forms, with or without
   * modification, are permitted provided that the following conditions are met:
   *
   * 1. Redistributions of source code must retain the above copyright notice, this
   *    list of conditions and the following disclaimer.
   * 2. Redistributions in binary form must reproduce the above copyright notice,
   *    this list of conditions and the following disclaimer in the
   *    documentation and/or other materials provided with the distribution.
   * 3. Neither the name of the copyright holder nor the names of its contributors
   *    may be used to endorse or promote products derived from this software without
   *    specific prior written permission.
   *
   * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
   * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
   * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
   * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
   * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
   * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
   *)
*/
