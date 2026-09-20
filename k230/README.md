# K230 texture measurement firmware

This directory contains the validated K230 production application for the
DataTouch texture measurement path.

## Runtime sequence

1. A debounced active-low key press on K230 GPIO21 starts one measurement.
2. K230 sends `LIGHT_ON` to STM32 and waits for its ACK.
3. The CSI1 frame is captured at `t = 2 s` after the ACK.
4. The light remains on until `t = 5 s`, then K230 sends `LIGHT_OFF`.
5. The frozen native `U_curve` pipeline evaluates the captured frame.
6. `timestamp,U_curve` is appended to `/sdcard/data_touch_results/u_curve.txt`.
7. K230 sends the little-endian float32 `U_curve` to STM32 and waits for ACK.
8. The application returns to `K230_DEPLOY_READY`.

The key handler uses 30 ms debounce and one-shot re-arming. A held key does
not repeat, and presses made while a measurement is busy are not queued.

## Hardware

- KEY: K230 GPIO21, active low.
- CSI: sensor ID 1, 1280 x 720 RGB565, frozen ROI `(160, 90, 960, 540)`.
- K230 GPIO11 / UART2 TX -> STM32 PA3 / USART2 RX.
- K230 GPIO12 / UART2 RX <- STM32 PA2 / USART2 TX.
- K230 GND <-> STM32 GND.
- UART2: 115200 baud, 8N1, 3.3 V TTL.

See `stm32_uart_protocol.md` for the binary frame definition and HW-269
wiring. Do not connect UART signals to 5 V.

## Native kernel requirement

The Python deployment requires the built-in CanMV module `gunay_native`.
The validated target is CanMV v1.8 at commit `c2d1f5c`, board configuration
`k230_canmv_01studio_defconfig`. Install `native/modgunay_native.c` as
`src/canmv/port/modules/modgunay_native.c` in that source tree before running
the normal CanMV build. Details are recorded in `native/build_environment.md`
and `native/native_interface.md`.

Large firmware images and build outputs are intentionally not tracked.

## Host checks

From the repository root:

```powershell
python -m unittest discover -s k230/tests -p "test_*.py" -v
```

The tests cover the frozen hardware/output contract, call order, framing,
checksum, float32 packing, timeout, retry, and receive resynchronization.

## Deployment

With the board's USB REPL available as COM14:

```powershell
powershell -ExecutionPolicy Bypass -File .\k230\upload_k230_final_deploy.ps1 `
  -Port COM14 -DeployRoot .\k230
powershell -ExecutionPolicy Bypass -File .\k230\start_k230_final_deploy.ps1 `
  -Port COM14 -TimeoutSeconds 30
```

The upload script positively checks the K230 board identity and the native
module before writing the production files.
