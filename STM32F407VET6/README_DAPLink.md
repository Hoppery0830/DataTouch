# DAPLink 烧录

适用本目录的 STM32F407VET6 主工程。DAPLink 须提供 CMSIS-DAP 调试接口。

## 接线

| DAPLink | STM32F407 |
| --- | --- |
| SWDIO | PA13 / SWDIO |
| SWCLK | PA14 / SWCLK |
| GND | GND |
| VTref（若有） | 3.3 V 参考电压 |

目标板正常供电。VTref 是电平参考，是否可由探针供电应按探针说明确认。默认配置使用系统软件复位，无需 NRST；不修改选项字节或读保护。

## VS Code

打开本目录或上一级 DataTouch.code-workspace：

1. 执行任务 **DAP: Flash Debug**：配置、编译、烧录 ELF、校验并复位运行。
2. 断点调试：安装 Cortex-Debug 扩展，选择 **STM32F407 - CMSIS-DAP (OpenOCD)** 并按 F5，程序停在 main。

烧录目标为本目录 `build/Debug/DataTouchCode.elf`。任务沿用本机已有的 OpenOCD 和 ARM 工具链路径；更换电脑后需调整 .vscode 配置。

## 命令行

在本目录执行（工具需在 PATH）：

```powershell
cmake --preset Debug
cmake --build --preset Debug
openocd -f openocd-daplink.cfg -c "program build/Debug/DataTouchCode.elf verify reset exit"
```

若连接不稳定，可将配置中的 `adapter speed 1000` 降至 `100` 后重试。烧录前退出占用探针的其他调试会话。
