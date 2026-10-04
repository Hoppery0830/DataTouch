# 六维力与 ADXL335 触觉采集整合

## 当前版本

采集逻辑来自 `Touch/STM32F407ZGT6_SixAxis_ADXL335_Integrated/STM32F407ZGT6_SixAxis_ADXL335_Integrated`。保留本工程 STM32F407VET6、168 MHz 时钟、512 KB Flash 链接脚本和现有电机/ESP32 控制；没有移植原工程的 ZG 型号启动配置。

根据本次接线决定，USART1 接电脑，暂不连接 K230。触觉采集上电后持续运行，与小程序 ARM/START/STOP 独立；STOP 仍只停止电机。本次没有新增触觉云端上传或舒适度评分。摩擦系数、滑动窗口特征及高通曲线沿用电脑端 Python 实现。

## 接线与资源

| 功能 | STM32 接口 | 参数 / DMA |
| --- | --- | --- |
| ADXL335 X/Y/Z | PC0/PC1/PC2，ADC1 IN10/11/12 | TIM3 TRGO，三通道扫描，DMA2 Stream0 Channel0 循环 |
| 六维力自动换向 RS485 模块 | PC10 = UART4 TX，PC11 = UART4 RX | 460800，8N1；RX DMA1 Stream2 Channel4 循环 |
| 电脑 USB-TTL | PA9 = USART1 TX → USB-TTL RXD，GND 共地 | 115200，8N1；TX DMA2 Stream7 Channel4 |
| X 电机 | UART5 PC12/PD2 | 原配置 |
| Z 电机 | USART6 PC6/PC7 | 原配置 |
| Yaw 电机 | USART2 PD5/PD6 | 原配置 |
| ESP32 控制链路 | USART3 PB10/PB11 | 原配置 |

ADXL335 由 3.3 V 供电并共地。六维力经现有自动换向 RS485 转 TTL 模块连接；模块标签可能以外部端口视角命名，以“模块输出送入 PC11，模块输入接 PC10”为准，沿用实测正确连线。没有新分配 DE 引脚，不适用于未经配置的手动 DE/RE 模块。PA10 不需要连接电脑；Z/R/M 按键在电脑软件内部处理，不向 F4 发送置零指令。K230 不要同时接到 USART1。

TIM2 仍为电机 500 Hz 控制节拍，TIM6 为 HAL 时间基准。触觉新增 TIM3：当前 APB1 定时器时钟 84 MHz，PSC=83、ARR=99，产生 10 kHz 触发。ADC 分频为 APB2/4=21 MHz，每通道 144 周期采样，完整三通道序列约 22.3 μs，小于 100 μs 触发周期。PB1 的 Hall 模拟引脚保留，但不加入本次三通道 DMA 序列。

## 数据处理和时间

- 三轴各 10 kHz，交错存储 X/Y/Z；100 帧为一块，每 10 ms 产生一次均值与去块均值后的三轴合成振动 RMS。
- DMA 半满/全满中断将完成半区复制进 4 块静态队列，任务消费副本。队列满或发现中断迟到时记录 `adc_overruns`，不让任务读取正在被 DMA 覆写的缓冲。
- 六维力支持 `53 54 + 6个小端float32 + CRC16` 主动帧及 `01 03 18 + 24字节数据 + CRC16` Modbus 回复。上电监听 2 秒；没有新鲜主动帧时每 10 ms 尝试发送原工程读六维寄存器命令。CRC 错误重同步，拒绝 NaN/Inf，半帧超过 50 ms 丢弃。
- 两类数据使用 F4 同一 `HAL_GetTick()` 毫秒时基，49.7 天回绕。A 时间为 DMA 块完成中断时间，F 时间为任务解析到完整帧的时间。它们是本地接收/块完成时间，不是传感器内部采样时间，也不保证两种传感器严格同时采样；中断和任务调度会引入延迟。
- 300 ms 没有新数据时快照标记离线，不把旧值当新样本持续输出。UART/ADC 硬件错误在任务中限频恢复，不在中断中阻塞。

标定值集中在 `source/config/touch_config.h`：ADXL335 的零点和灵敏度、六维力 Fx/Fy/Fz 固定零偏来自原示例。F4 输出力已经扣除这三项零偏，力矩没有扣零偏。上位机脚本另有很小的力矩预置偏移；不要在多端重复扣除力零偏。更换传感器或安装方向后需重新确认标定和 `--normal-axis`，不能将这组数当所有硬件的通用标定。

## 电脑输出与使用

串口为 115200、8 数据位、无校验、1 停止位。保留原 F/A/S 格式：

```text
F,time_ms,Fx_mN,Fy_mN,Fz_mN,Mx_mNm,My_mNm,Mz_mNm,ACTIVE或MODBUS,frames
A,time_ms,ax_mg,ay_mg,az_mg,vibration_rms_mg,blocks
S,time_ms,force_frames,crc_errors,uart_errors,ring_overflows,adxl_overruns,adc_errors
```

块统计仍为 100 Hz，F/A 每 20 ms 最多发送最新结果，即默认输出上限 50 Hz；S 为 1 Hz。这样为 115200 文本流留出带宽。ADC 块号每行通常增长 2 属于有意抽取，不是采样丢块；真实采集丢块看 S 中 `adxl_overruns`。这不是 50 Hz 重采样抗混叠滤波，不能用输出均值序列分析 25 Hz 以上波形；块 RMS 是原始 10 kHz 样本计算的包络特征，也不能替代原始波形 FFT。发送忙时丢弃当前输出尝试，后续发送最新值，避免积压阻塞电机。

原绘图脚本已复制到本工程 `tools/SixAxis_ADXL335_realtime_plot.py`，无需依赖原 Touch 文件夹。在本工程目录运行：

```powershell
python -m pip install pyserial matplotlib
python tools/SixAxis_ADXL335_realtime_plot.py --port COM8 --baud 115200
```

COM8 替换为实际 USB-TTL 端口；先关闭占用同一端口的串口助手。脚本会显示力、切向力/摩擦系数、力矩和振动，并保存数据/特征 CSV 与曲线 PNG。Z 软件去零，R 恢复脚本预置偏移，M 重置摩擦系数平均值。

Ozone 可查看 `g_touch_status`：`force_online/adc_online`、原始六维力 `force.values`、ADC 均值 `adxl.raw`、加速度 `adxl.mg`、振动 `rms_mg`、ADC 饱和 `clipped`、CRC/无效浮点数计数及 `hardware` 错误计数。`hardware.pc_drops/pc_errors` 单独记录电脑发送问题，不等同传感器丢样。

## 构建与验证

```powershell
cmake --preset Debug
cmake --build --preset Debug
python tools/check_integration.py
python tests/run_tests.py --cc <电脑端gcc.exe路径>
python tests/run_touch_tests.py --cc <电脑端gcc.exe路径>
```

烧录 `build/Debug/DataTouchCode.elf`（同目录也生成 HEX/BIN）。主机测试需要 Windows/本机 GCC，不能使用 arm-none-eabi-gcc；MinGW 安装路径若含中文导致链接库找不到，可通过 ASCII 路径的目录联接调用并使用测试脚本提供的 `-no-canonical-prefixes`。

上板依次确认：静止时 A 的块号持续增长、姿态改变时重力轴变化；六维力有新 F 帧且卸载/加载响应合理；S 中 CRC、UART、环形溢出、ADC 溢出及错误不持续增长；再从小程序运行三个模式，观察控制应答及采集连续性。还需实机检查栈高水位和长期采样稳定性。软件测试不覆盖接线、传感器供电及真实 RTOS/DMA 时序。

## ADXL 数据固定或曲线只有零线时

先查看 `g_touch_status.adxl.raw[0..2]`（X/Y/Z）和 `g_touch_status.adxl.clipped`。
当前标定下，三轴 ADC 均值4095分别换算为5087、5079、5099mg；若这些值一直不变，
高通动态分量和去均值 RMS 都会为0，这不等于电脑没收到数据，也不能据此断言传感器损坏。
比较模块 XOUT/YOUT/ZOUT 与 F4 PC0/PC1/PC2 相对共地的电压，确认实际信号通路。

隔离测试可先断电，断开模块 XOUT 与 PC0 的连接，再把 F4 PC0 接到 F4 GND，重新上电运行；
此时 `adxl.raw[0]` 应接近0。不要把仍连接着的传感器输出直接短路到地。
该测试主动将输入置于下限，所以 `clipped=true` 是预期现象，不作为测试失败依据。
测试后断电拆除接地线并恢复 XOUT。接地仍读满量程时，优先核对实际引脚、模拟参考供电和 ADC 配置。

电脑端时间计算已处理首批 F/A 跨传感器时间戳的小幅乱序，避免先接收的力帧比 ADC 块时间晚几毫秒时，
把 ADC 相对时间误算为49.7天。修正版允许小幅负时间；一次记录时长应小于约24.9天。
回归测试运行 `python tests/test_plot_time.py`。修改仅涉及上位机时，重启脚本即可，不需重刷固件。

## 极客 STM32F407VET6 核心板 ADC 参考电压

使用板载 3.3V 作为 ADC 参考时，按核心板配套资料连接 REF+ 与 3.3V、REF− 与 GND 的对应焊盘。必须断开所有供电后操作，只连接各自一对焊盘，不能将 REF+ 与 REF− 短接；使用外部参考源时不能同时桥接到 3.3V。

本次实机排查发现 REF+ 至 3.3V 未连通。补接后，PC0 接地测试的原始值为 0～7；恢复 ADXL335 后三轴脱离满量程，晃动时三轴动态曲线和块内 RMS 均有响应。这验证了采集链路恢复，不代表零偏、灵敏度及噪声已完成标定。

## CubeMX 再生成

触觉 BSP 在 MX 外设初始化之后、TouchTask 启动时配置 ADC 扫描、TIM3、UART4 波特率及 DMA；这些运行期设置不由 `.ioc` 表示。因此 CubeMX 界面可能仍显示 ADC 软件触发、UART4 115200，以 `Touch_BSP_Start()` 的运行期配置为准。

保持 `.ioc` 的既有 GPIO 和外设映射及 Keep User Code；**不要在 CubeMX 再创建 TouchTask，或重复启用本 BSP 拥有的 UART4/USART1/ADC/DMA 中断及 DMA Stream**。若将来决定把这些资源迁回 CubeMX 管理，应同时删除 BSP 对应初始化及 IRQ 定义。生成后运行 `tools/check_integration.py` 检查重复 IRQ、任务和回调接入，再完整构建。

代码入口：[采集任务](source/task/Touch_Task/README.md)、[硬件传输](source/bsp/touch/README.md)、[协议与计算](source/module/touch/README.md)。
