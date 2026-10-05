# K230 native firmware source and build

The base is official CanMV K230 v1.8, revision `c2d1f5c`, from the
`canmv_v1.8.xml` manifest. Use board configuration
`k230_canmv_01studio_defconfig` in the complete K230 build environment.

Apply these files to the manifest's `src/canmv` checkout:

| Published file | Destination within `src/canmv` |
| --- | --- |
| `modgunay_native.c` | `port/modules/modgunay_native.c` |
| `canmv_port/port/core/main.c` | `port/core/main.c` |
| `canmv_port/port/machine/modmachine.c` | `port/machine/modmachine.c` |
| `canmv_port/port/omv/ide_dbg.c` | `port/omv/ide_dbg.c` |

The three port replacements are also available as
`canmv_v1.8_state_management.patch`, generated against the unmodified release
revision. Use the patch or the port replacements, then install the native
module. The normal v1.8 build collects `port/modules/*.c` automatically.

From the complete manifest build root, configure the 01Studio board using the
upstream build procedure, install its RISC-V toolchains, and run `make canmv`.
Linux/WSL ext4 is the preferred build location for intermediate files.

The port changes expose `machine.ide_connected()`, keep IDE attachment and
disconnect observational, make explicit script stop take over the runtime,
and consume `/sdcard/.datatouch_mode_reset` for deliberate mode changes.
They are required by the current Python application.

Before application deployment, verify board identity
`k230_canmv_01studio with K230`, successful `import gunay_native`,
`gunay_native.region_stats_fused_span_query`, and `machine.ide_connected()`.
CanMV is an RT-App in the image's boot partitions; Python uploads to
`/sdcard` do not install the native firmware.

This repository contains source and rebuild instructions. Firmware images,
toolchain downloads, compiler output, board logs and calibration images are
not included. Publication checks cover Python regressions, script syntax and
the STM32F1 build. Full CanMV image build and board acceptance are separate.
