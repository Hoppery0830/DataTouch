# K230 native build environment

## Frozen target

- Board identity required before deployment: `k230_canmv_01studio with K230`.
- Firmware lineage: CanMV v1.8, release commit `c2d1f5c`.
- Source manifest: official `canmv_v1.8.xml`.
- Board configuration: `k230_canmv_01studio_defconfig`.

## Available and verified components

- WSL2 Ubuntu 22.04, Linux 6.6.87.2.
- Official CanMV v1.8 manifest and synchronized source tree at `/home/zhm/k230_native_build`.
- Official K230 RT-Smart and Linux RISC-V toolchains installed by `make dl_toolchain`.
- GNU make, GCC/G++, CMake, Git, repo, SCons 3.1.2, and the documented Python build dependencies.
- Native source installed at `src/canmv/port/modules/modgunay_native.c`; the v1.8 Makefile collects it through `modules/*.c`.
- Successful `make canmv` build for the frozen 01Studio configuration.
- Linked executable size: 15,799,816 bytes; linked binary contains the `gunay_native` module registration string.
- Deployable artifact: `analysis/k230_native_region_stats/build/micropython_canmv_v1.8_01studio_gunay_native`.

The build root is on WSL ext4 so compilation does not create high-frequency intermediate writes in the project directory. Only the final executable is retained in the project.

## Flash and board execution

- COM14 positive identity was reconfirmed as CanMV v1.8 on `k230_canmv_01studio with K230` before preparing the image.
- The CanMV executable is a fast-boot RT-App stored in the hidden `rtapp_a` and `rtapp_b` image partitions; it is not a replaceable file in the mounted `/sdcard` data partition.
- Full official image: `analysis/k230_native_region_stats/build/CanMV_v1.8_01Studio_gunay_native_full.img.gz`.
- Image size: 20,793,876 bytes; gzip integrity passed; MD5 is recorded beside the image.
- The image's SDCARD partition contains `texture_structure_tensor_v1`, the native runner, and all 15 frozen test images.

- Windows WinUSB was installed only for the K230 boot device `VID_29F1&PID_0230`; the official CLI then identified exactly one BROM device.
- The uncompressed 650,117,120-byte image was written to SDCARD to 100%, and the CLI reported flash completion.
- After a normal power cycle, COM14 identified the flashed target as `k230_canmv_01studio with K230`; importing the built-in `gunay_native` module passed.
- The golden-image native run completed on the physical board. Build and execution environment are therefore unblocked.
