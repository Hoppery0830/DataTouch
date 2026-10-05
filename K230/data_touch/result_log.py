"""Numbered SD measurements with bounded-memory recovery and append checks."""

import os

from data_touch.config import RESULT_DIRECTORY, RESULT_TXT


_state = None
_MAX_LINE = 512
_COPY_CHUNK = 1024


def _stat(path):
    try:
        return os.stat(path)
    except OSError as error:
        if error.args and error.args[0] == 2:  # ENOENT only
            return None
        raise


def ensure_directory(path):
    info = _stat(path)
    if info is None:
        os.mkdir(path)
    elif not info[0] & 0x4000:
        raise OSError("not a directory: %s" % path)


def unused_path(path):
    candidate = path
    suffix = 1
    while _stat(candidate) is not None:
        candidate = "%s.%d" % (path, suffix)
        suffix += 1
    return candidate


def _write(handle, data):
    if handle.write(data) != len(data):
        raise OSError("short SD write")


def _finite(value):
    return not (value != value or abs(value) == float("inf"))


def _record(raw, expected_index):
    fields = raw.decode("ascii").strip().split(",")
    if len(fields) != 2:
        raise ValueError("expected measurement_index,U_curve")
    label, score = fields
    legacy = False
    if label.isdigit():
        if int(label) != expected_index:
            raise ValueError("expected measurement index %d" % expected_index)
    elif (len(label) == 19 and label[4] == "-" and label[7] == "-"
          and label[10] == " " and label[13] == ":" and label[16] == ":"
          and label.replace("-", "").replace(" ", "").replace(":", "").isdigit()):
        legacy = True
    else:
        raise ValueError("invalid measurement index")
    value = float(score)
    if not _finite(value):
        raise ValueError("non-finite U_curve in SD log")
    if score != "%.6f" % value:
        raise ValueError("incomplete or noncanonical U_curve")
    return value, legacy


def _scan():
    """Scan once, retaining only one bounded line and the committed prefix."""
    info = _stat(RESULT_TXT)
    if info is None:
        return {"count": 0, "size": 0, "newline": False, "legacy": False, "repair": False}
    count = valid_size = 0
    legacy = newline = repair = False
    handle = open(RESULT_TXT, "rb")
    try:
        while True:
            raw = handle.readline(_MAX_LINE)
            if not raw:
                break
            at_end = handle.tell() == info[6]
            try:
                if len(raw) == _MAX_LINE and not raw.endswith(b"\n"):
                    raise ValueError("SD record exceeds line limit")
                _, old_format = _record(raw, count + 1)
            except (ValueError, UnicodeError) as error:
                if at_end and not raw.endswith(b"\n"):
                    repair = True  # Interrupted trailing write; preserve it in a backup.
                    break
                raise ValueError("invalid SD record %d: %s" % (count + 1, error))
            count += 1
            valid_size = handle.tell()
            legacy = legacy or old_format
            newline = not raw.endswith(b"\n")
    finally:
        handle.close()
    return {"count": count, "size": valid_size, "newline": newline, "legacy": legacy, "repair": repair}


def _copy_prefix(source, target, length):
    reader = open(source, "rb")
    try:
        writer = open(target, "wb")
        try:
            remaining = length
            while remaining:
                data = reader.read(min(_COPY_CHUNK, remaining))
                if not data:
                    raise OSError("SD source shortened during recovery")
                _write(writer, data)
                remaining -= len(data)
            writer.flush()
        finally:
            writer.close()
    finally:
        reader.close()


def _recover_replacement():
    previous = RESULT_TXT + ".replace_previous"
    if _stat(previous) is None:
        return
    temporary = RESULT_TXT + ".pending"
    if _stat(RESULT_TXT) is None:
        if _stat(temporary) is not None:
            os.rename(temporary, RESULT_TXT)
        else:
            os.rename(previous, RESULT_TXT)
            print("K230_RESULT_LOG_REPLACE_RESTORED")
            return
    archived = unused_path(RESULT_TXT + ".recovered_previous")
    os.rename(previous, archived)
    print("K230_RESULT_LOG_REPLACE_RECOVERED path=%s" % archived)


def _replace_log(temporary, backup_suffix):
    """Keep the original, then replace the primary file in one rename."""
    info = _stat(RESULT_TXT)
    if info is not None:
        backup = unused_path(RESULT_TXT + backup_suffix)
        _copy_prefix(RESULT_TXT, backup, info[6])
        print("K230_RESULT_LOG_BACKUP path=%s" % backup)
    replace = getattr(os, "replace", None)
    if replace is not None:
        replace(temporary, RESULT_TXT)
    elif info is None:
        os.rename(temporary, RESULT_TXT)
    else:
        # This CanMV/RT-Smart build returns EEXIST on rename-over-existing.
        # The fixed previous path lets startup recover either interrupted step.
        previous = RESULT_TXT + ".replace_previous"
        os.rename(RESULT_TXT, previous)
        try:
            os.rename(temporary, RESULT_TXT)
        except Exception:
            if _stat(RESULT_TXT) is None:
                os.rename(previous, RESULT_TXT)
            raise
        os.remove(previous)


def _normalize(state):
    temporary = RESULT_TXT + ".pending"
    reader = open(RESULT_TXT, "rb")
    try:
        writer = open(temporary, "wb")
        try:
            for index in range(1, state["count"] + 1):
                value, _ = _record(reader.readline(_MAX_LINE), index)
                _write(writer, ("%d,%.6f\n" % (index, value)).encode("ascii"))
            writer.flush()
        finally:
            writer.close()
    finally:
        reader.close()
    _replace_log(temporary, ".legacy" if state["legacy"] else ".recovered")


def initialize_log(force=False):
    """Continue numbering after reset; migrate old timestamps only once."""
    global _state
    if force or _state is None:
        ensure_directory(RESULT_DIRECTORY)
        _recover_replacement()
    info = _stat(RESULT_TXT)
    size = 0 if info is None else info[6]
    if force or _state is None or size != _state["size"]:
        _state = None
        state = _scan()
        if state["legacy"] or state["repair"]:
            _normalize(state)
            state = _scan()
        _state = state
    return _state["count"]


def line_count():
    """Compatibility name: cached count of validated measurements."""
    return initialize_log()


def next_measurement_index():
    return initialize_log() + 1


def _rollback(size):
    info = _stat(RESULT_TXT)
    if info is not None and info[6] != size:
        temporary = RESULT_TXT + ".pending"
        _copy_prefix(RESULT_TXT, temporary, size)
        _replace_log(temporary, ".failed")


def append_u_curve(value, expected_index=None):
    """Commit one checked record; failed writes do not consume an index."""
    global _state
    value = float(value)
    if not _finite(value):
        raise ValueError("cannot save non-finite U_curve")
    index = next_measurement_index()
    if expected_index is not None and int(expected_index) != index:
        raise RuntimeError("measurement index changed before save")
    state = _state
    line = "%d,%.6f\n" % (index, value)
    data = (("\n" if state["newline"] else "") + line).encode("ascii")
    try:
        handle = open(RESULT_TXT, "ab")
        try:
            _write(handle, data)
            handle.flush()  # CanMV POSIX flush calls fsync; failures must propagate.
        finally:
            handle.close()
        if _stat(RESULT_TXT)[6] != state["size"] + len(data):
            raise OSError("SD append size mismatch")
        reader = open(RESULT_TXT, "rb")
        try:
            reader.seek(state["size"])
            if reader.read(len(data)) != data:
                raise OSError("SD append readback mismatch")
        finally:
            reader.close()
    except Exception as error:
        _state = None
        try:
            _rollback(state["size"])
        except Exception as rollback_error:
            raise RuntimeError("SD save failed (%s); rollback failed (%s)" % (error, rollback_error))
        raise
    _state = {"count": index, "size": state["size"] + len(data), "newline": False,
              "legacy": False, "repair": False}
    return line.rstrip()
