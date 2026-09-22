# RTOS 对象与任务关系

`app_rtos.c/.h` 集中管理两个静态队列，并实现栈溢出、内存分配失败钩子。队列的控制块和存储区均为静态对象。

| 对象 | 容量 | 消息 | 消费者 |
|---|---|---|---|
| g_motor_commands | 4 | motor_command_t，按值复制完整路径 | MotorTask |
| g_buzzer_commands | 4 | buzzer_pattern_t | ServiceTask |

初始化链：`osKernelInitialize()` → `MX_FREERTOS_Init()` 的 USER CODE Init → `App_RTOS_Objects_Init()` → CubeMX 创建两个线程 → 检查句柄 → `osKernelStart()`。

| 任务 | 优先级 | 栈 | 调度 |
|---|---|---|---|
| MotorTask | osPriorityAboveNormal1 | 1024 Words = 4096B | UART/节拍/命令/停止标志，最多等 2 tick |
| ServiceTask | osPriorityLow | 512 Words = 2048B | 每 5 tick 推进服务 |

当前 tick=1000Hz，两个任务均静态分配，CubeMX 入口选 Weak。正式强定义在各自目录，不重复创建任务。内核仍允许动态分配，静态业务队列不代表整个系统完全不用堆。

## 并发边界

外部任务使用公开 Submit/Beep/Stop/Mute/GetStatus API。它们是任务上下文接口，不能直接由 ISR 调用。UART/TIM2 中断只使用专用通知桥接，并配置满足 FreeRTOS 限制的优先级 6。

队列满时普通提交返回 false；停止和取消/静音使用独立标志，不受队列满限制。队列重置会丢弃此前排队的消息，提交成功不保证后来未被取消。

`vApplicationStackOverflowHook`、`vApplicationMallocFailedHook` 调用 Error_Handler。目前错误处理是关中断后停留，不执行电机通信，不保证外部驱动器停止；调试时通过断点定位。

## 再生成保护

`App_RTOS_Objects_Init` 调用和头文件引入在 CubeMX 原有 USER CODE 区域。保持 Keep User Code 选项；不要把任务创建或队列创建复制到多个入口。根 CMake 接入 source，生成的中间 CMake 不手改。

运行 `python tools/check_integration.py` 检查接入，再编译。任务栈初值需结合实机高水位调整。
