"""Persistent mode dispatcher; keep hardware side effects out of this file."""

from mode_config import DEBUG, DEBUG_IDLE, blink_debug_start, get_mode, report_runtime_state


mode = get_mode()
if mode == DEBUG:
    blink_debug_start()
    report_runtime_state(DEBUG_IDLE, "PERSISTED_DEBUG_MODE")
    print("K230_DEBUG_READY mode=DEBUG")
else:
    from production_main import main

    main()
