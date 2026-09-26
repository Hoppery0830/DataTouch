# DataTouch

DataTouch 包含 STM32 主控、电机控制、ESP32 联网网关、K230 视觉处理和微信小程序。当前电机控制链路为：小程序与云函数 → ESP32 → STM32F407VET6 → 电机。

## 仓库结构

| 目录 | 用途 | 工程入口 |
| --- | --- | --- |
| `STM32F407VET6/` | F4 主控、FreeRTOS 任务、电机及外设驱动 | `CMakeLists.txt`、CubeMX `.ioc` |
| `STM32F1/` | F1 软件原型，后续迁移到 F4 | `CMakeLists.txt`、CubeMX `.ioc` |
| `K230/` | 视觉处理、部署脚本和测试 | `README.md` |
| `ESP32/` | 串口通信、Wi-Fi、网页和云端通信 | `platformio.ini`、`README.md` |
| `miniprogram/` | 微信小程序及云函数 | `project.config.json`、`README.md` |
| `docs/` | 跨端协议、系统架构、供电及技术方案 | `电机联调.md` |

## 打开和构建

- ESP32：使用 VS Code / PlatformIO 打开 `ESP32/`，或在仓库根目录执行 `pio run -d ESP32`。烧录时执行 `pio run -d ESP32 -t upload`。
- F4 / F1：分别打开相应目录，使用 ARM GCC、CMake 和 Ninja。在工程目录执行 `cmake --preset Debug`，然后执行 `cmake --build --preset Debug`。
- 小程序：微信开发者工具导入 `miniprogram/`，其中包含 `miniprogram/` 页面源码和 `cloudfunctions/` 云函数。
- K230：部署与测试命令见 [K230 说明](K230/README.md)。
- 多工程浏览：打开根目录 `DataTouch.code-workspace`；各工程保留自己的构建配置。

## 目录迁移说明

原 `stm32_f407/` 改为 `STM32F407VET6/`，原 `stm32/` 改为 `STM32F1/`，原 `k230/` 改为 `K230/`。原根目录的 `src/`、`include/`、`lib/`、`test/`、`platformio.ini` 和 ESP32 编辑器配置移入 `ESP32/`。

历史技术方案中的源码路径以编写时目录为准，查阅时按上述映射定位。迁移前生成的构建缓存可能含绝对路径，不能跨目录直接复用；请在新工程目录重新配置和构建。

公共资料放在 `docs/`，各端使用说明与源码放在对应工程目录。生成文件、编译缓存及文档渲染中间文件不纳入版本控制。
