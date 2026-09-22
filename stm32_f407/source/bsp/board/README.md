# board：板级接入

文件：`board.c/.h`。将 CubeMX 初始化流程与自建模块连接，不重复定义 HAL 定时器总回调。

## 接入顺序

1. main 完成 HAL、时钟、GPIO、DMA、串口及 TIM2 初始化。
2. main 的 USER CODE 2 调用 `Board_Init()`，启用 DWT 并关闭蜂鸣器。
3. FreeRTOS 创建 MotorTask；任务绑定三路 UART 后调用 `Board_StartControlTick()`。
4. TIM2 IRQ → `HAL_TIM_IRQHandler` → main 中 `HAL_TIM_PeriodElapsedCallback` → `Board_TimerCallback()`。

TIM6 的分支继续调用 `HAL_IncTick()`；board 仅处理 TIM2：维护 DWT 并发出任务节拍通知。

| 接口 | 上下文 | 行为 |
|---|---|---|
| Board_Init | 调度器启动前 | 初始化 DWT、关蜂鸣器 |
| Board_StartControlTick | MotorTask 启动阶段 | 清计数/更新标志后开启 TIM2 中断 |
| Board_TimerCallback | HAL 定时器 ISR | 分发 TIM2，不处理其他定时器 |

## 配置依赖

TIM2 当前时钟 84MHz、PSC=83、ARR=1999，得到 500Hz；IRQ 优先级为 6，满足 RTOS ISR 调用约束。不能只改 `APP_CONTROL_HZ` 就期待硬件同步变化，仍须配置 CubeMX 的 TIM2。

保持 main 的 Includes、USER CODE 2、Callback 1 三处接入。重新生成后运行 `python tools/check_integration.py` 并编译。自建 board 不实现其他外设采集初始化。
