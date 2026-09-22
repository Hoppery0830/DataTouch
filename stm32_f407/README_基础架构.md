> 当前为三端电机联调固件：上电不运动，选择模式 → ARM → START；详见 [联调说明_小程序_ESP32_STM32.md](联调说明_小程序_ESP32_STM32.md)。以下旧演示说明不代表当前启动配置。

> 当前固件默认上电执行一次按压，旧往复自检关闭。开关：APP_MOTOR_AUTOSTART=1、APP_AUTOSTART_MODE=ACTION_PRESS、APP_MOTOR_SELFTEST=0。详见 source/task/Motor_Task/STARTUP.md（工程根目录下）。

# DataTouch STM32 基础架构

工程：STM32F407VET6，CubeMX HAL + FreeRTOS CMSIS-RTOS V2，C11/CMake。
本阶段实现三轴电机和有源蜂鸣器。ADC、SPI、其他串口仅保留 CubeMX 初始化，不启动采集或通信业务。

## 配置核对与修正

| 用途 | 外设 / 引脚 | 配置 |
|---|---|---|
| Motor_X | UART5，PC12 TX / PD2 RX | 115200，8N1，RX 循环 DMA，TX 普通 DMA |
| Motor_Z | USART6，PC6 TX / PC7 RX | 同上 |
| Motor_Yaw | USART2，PD5 TX / PD6 RX | 同上 |
| 有源蜂鸣器 | PE3 / BEEP | GPIO；当前按高电平响、低电平关配置 |
| HAL 时基 | TIM6 | 1ms |
| 电机路径节拍 | TIM2 | APB1 定时器时钟 84MHz，PSC=83，ARR=1999，500Hz；IRQ=6 |
| RTOS | SysTick | 1000Hz；MotorTask 4096B、ServiceTask 2048B，均静态、Weak 入口 |

当前 `SystemClock_Config` 使用 **HSI 16MHz → PLL → 168MHz**，没有启用 HSE。
`.ioc` 的 HSE_VALUE=25MHz 不是板上晶振实测结果。当前 PLLQ 输出 84MHz，未配置 USB/SDIO/RNG 的 48MHz 时钟；本阶段未使用这些外设。
原始关键文件保存在 `backup_before_framework/`。
UART4、USART3 目前仍为 CubeMX 的 115200，后续接力传感器/ESP32 时须按各端协议重新配置；本阶段不消费它们。

## 分层

- `source/bsp/uart`：三路独立 DMA/TX 缓冲，字节环形缓冲，IDLE/HT/TC 收取新增字节，错误计数与恢复。
- `source/bsp/dwt`：64 位单调微秒时间；短临界区保护扩展，TIM2 定期维护回绕。
- `source/bsp/board`：初始化和定时器回调桥接。
- `source/bsp/buzzer`：有源蜂鸣器 GPIO 开关，不占 PWM。
- `source/module/motor`：Emm 编解码、使能、位置/速度、停止、清零、查询、反馈、事务超时与故障。
- `source/module/buzzer`：非阻塞通断节奏、次数、取消和静音。
- `source/task/Motor_Task/motor_state_machine`：**运动路径**模式：点到点、两点往复、多点序列；不是底层电机模式。
- `motor_handle`：逐点下发、等待新反馈确认到位、停留；不会延时等待电机。
- `axis_control_drv`：X/Z 的 mm、mm/s；Yaw 的 °、°/s，与 Emm 参数换算。
- `motor_config`：机械换算、方向、速度和行程限制。
- `source/task/app_rtos`：集中静态创建电机命令队列与蜂鸣器队列。

厂家例程仅作为报文依据，未复制其写死 huart1 的静态发送函数和阻塞等待流程。
Emm 采用示例的 **固定 0x6B 校验模式**，不使用 CRC16；驱动器设置须相同。
速度命令为 RPM，位置命令为脉冲；实时位置反馈为有符号幅值，**65536 表示一圈**。
三路串口独立，因此三台电机地址都暂设为 1。

## 启动与调用

上电行为由 `APP_MOTOR_AUTOSTART` 和 `APP_MOTOR_SELFTEST` 控制，两者不能同时开启。当前启动动作开启、旧自检关闭：Z 在线后使能、当前位置设零，执行一次按压并返回，详见 [启动说明](source/task/Motor_Task/STARTUP.md)。电机初始化等待 600ms；蜂鸣器不自动发声。
所有 module/motor 和路径内部函数只由 MotorTask 调用；其他任务通过下面的队列 API 提交。

1. 用 `MotorTask_GetStatus()` 检查对应轴 `online`。
2. 提交 `MOTOR_CMD_ENABLE`；等待使能应答和随后状态回读，确认 `enabled`。
3. 轴静止且位于选定参考位置时，提交 `MOTOR_CMD_ORIGIN`；等待 `origin_valid && online`。
   **这是当前位置清零，不是寻找机械原点**，没有实现限位开关回零。
4. 提交路径，观察 `path_status`、`point`、`cycle`。
5. `MotorTask_Stop()` 独立于命令队列发出停止请求，并清除旧路径命令。

以下片段用于应用层任务，不能直接放在上电初始化中自动执行：

```c
#include "motor_task.h"
#include "service_task.h"

/* 第一步：提交使能，返回 true 只表示成功入队。 */
motor_command_t enable = {
    .kind = MOTOR_CMD_ENABLE, .axis = MOTOR_X, .enable = true
};
bool queued = MotorTask_Submit(&enable);

/* 确认轴静止、原点正确后，单独提交，并等待反馈。 */
motor_command_t origin = {.kind = MOTOR_CMD_ORIGIN, .axis = MOTOR_X};
/* MotorTask_Submit(&origin); */

/* enabled、origin_valid、online 均满足后，再提交 1mm 绝对位置路径。 */
motor_command_t move = {.kind = MOTOR_CMD_PATH};
move.path.mode = PATH_POINT_TO_POINT;
move.path.axis_mask = 1U << MOTOR_X;
move.path.point_count = 1;
move.path.cycles = 1;
move.path.segment_timeout_ms = 10000;
move.path.points[0].target[MOTOR_X] = 1.0f;
move.path.points[0].speed[MOTOR_X] = 1.0f;
/* MotorTask_Submit(&move); */

/* 有源蜂鸣器响 100ms、停 100ms，共 2 次。 */
ServiceTask_Beep((buzzer_pattern_t){100, 100, 2});
```

`PATH_RECIPROCATING` 固定两个点 A、B，`cycles` 表示执行 A→B 的次数：到 A→到 B→到 A→到 B……，最终停在 B。
`PATH_WAYPOINTS` 支持最多 8 个点，按顺序执行，每轮最后一点之后直接进入下一轮第一个点。
路径的每个点可选择多个轴，等待所有选中轴的新位置/状态反馈达到目标后进入停留或下一点。
**多轴通过独立串口下发，不保证同时起步、直线插补或相同到达时间。** 平面扫描可用点序列描述，精确同步/插补后续扩展。
路径每段超时包含命令等待、运动和停留时间。反馈窗口为 300ms，查询事务间隔为 20ms，位置/状态/速度轮流查询；500Hz 是路径检查节拍，不是串口回读频率。

## 故障与状态语义

- `MotorTask_Submit` 返回值只表示入队；`last_command_ok` 表示任务接受/成功下发，**不是电机 ACK 或运动完成**。
- 路径完成用 `path_status=PATH_DONE`；使能和原点状态用反馈字段判断。数值位置需配合 `online` 使用。
- 单路只允许一个等待响应事务。超时、串口错误或溢出会锁存故障、使原点无效并请求停止；不自动重放运动指令。
- 通信恢复后查询继续，但故障需 `MOTOR_CMD_CLEAR_FAULT` 显式清除，再重新确认原点。
- 停止失败由 `stop_failed` 表示，先显式再次停止并取得 ACK，才可清故障。
- 活动路径遇到过期反馈、异常电机状态、段超时或合并/丢失控制节拍，会中止并向三轴发停止。
- `MotorTask_Stop` 是软件串口停止，不能替代硬件急停；驱动器失联时不能保证停机。
- `ServiceTask_SetMuted(true)` 取消当前及排队提示音；解除静音不恢复旧提示。服务周期为 5ms。
- `control_ticks`、`missed_ticks`、`max_iteration_us`、命令计数通过状态快照读取。模块对象保留超时/拒绝计数，BSP 保留错误/溢出计数。

## 必须与实物核对的参数

`source/task/Motor_Task/motor_config.c`：

- X/Z 暂设 1mm/电机转、3200 脉冲/转、0–50mm、最高 8mm/s。
- Yaw 暂设直驱 360°/电机转、3200 脉冲/转、±180°、最高 90°/s；有减速机构必须修改。
- 三轴默认方向未反转，Emm 加速度参数暂设 10（厂家参数，不是物理加速度）。
- 速度以整数 RPM 下发，存在量化；最小非零有效速度取决于传动比。
- 蜂鸣器暂按高电平有效，与当前 CubeMX 初始低电平一致。若实际低有效，同步修改 `app_config.h` 和 CubeMX PE3 初始输出电平。

还未进行上板、运动、蜂鸣器极性和驱动器实际固件验证。HSI 串口精度、DMA 中断延迟和 500Hz 最坏执行时间需上板测量。
循环 DMA 需在半缓冲时间内服务中断，不能长时间屏蔽中断；当前 128B、115200 波特率下半缓冲约 5.56ms。
固定校验字节协议本身没有 CRC 检错能力，也没有事务序号；解析器按地址/功能/长度/尾字节匹配，不声称能识别所有字节损坏或任意迟到的同功能响应。

## CubeMX 重新生成

已同步 `.ioc`：TIM2=500Hz/IRQ6，ServiceTask 静态分配。保留 `Keep User Code when re-generating`。

- `source/` 和根目录 CMake 自建内容保持独立；CubeMX 生成的 `cmake/stm32cubemx/CMakeLists.txt` 没有手改。
- `main.c`：Includes、USER CODE 2、Callback 1 中挂接 BSP。
- `freertos.c`：Includes、Init、RTOS_THREADS 中创建队列并检查线程创建结果。
- `FreeRTOSConfig.h`：USER CODE Defines 中启用栈溢出和分配失败检查。
- 两个自定义强符号任务入口位于 `source/task`；CubeMX 中继续选择 Weak。

重新生成后运行：

```powershell
python tools/check_integration.py
cmake --preset Debug
cmake --build --preset Debug
```

本次已做代码级接入检查，未通过 CubeMX GUI 实际重新生成一次。若再次生成时改变文件组织或删除任务，须重新核对接入点。

## 构建与测试

ARM GCC、CMake、Ninja 需在 PATH 中。构建命令同上，产物在 `build/Debug`。
本机回归测试运行实际的协议、UART BSP、motor、轴换算、路径和蜂鸣器代码，HAL 使用小型桩件，不模拟 FreeRTOS 调度或电气时序：

```powershell
python tests/run_tests.py --cc C:/toolchains/mingw64/bin/gcc.exe
```

测试包括报文黄金向量、半帧/粘帧/错帧、正负位置、单位及范围、DMA 回绕/重复事件/溢出、ACK 与到位区分、超时不重发、停止失败、路径新反馈门控、往复次数及蜂鸣器静音/时间回绕。
Windows MinGW 路径建议使用 ASCII；本次测试以 build/host-toolchain 目录联接访问本机现有工具链，不属于交付源码。

## 分模块中文说明

新增 [source 阅读索引](source/README.md)，包含全部 BSP、module、任务、配置及自检的接口、调用顺序和限制。代码头文件同步补充中文接口约定。

## 固定动作

已接入按压、揉搓和滑动三个有限动作，通过 `MotorTask_RunAction()` 提交。默认参数、调用前提和阶段状态见 [固定动作说明](source/task/Motor_Task/ACTIONS.md)。
