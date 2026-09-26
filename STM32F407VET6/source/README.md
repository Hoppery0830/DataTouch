> 当前上电演示关闭，已加入 [远程控制](module/remote/README.md)、[通信任务](task/Remote_Task/README.md) 和 [USART3 BSP](bsp/esp_uart/README.md)。当前启动语义以工程根目录三端联调说明为准。

# 自建代码阅读索引

本目录是 STM32F407VET6 的自建代码，CubeMX 负责 Core/Drivers/Middlewares，根 CMake 通过 `add_subdirectory(source)` 接入这里。


## 阅读顺序

1. [配置](config/README.md)：先确定硬件映射、单位和测试开关。
2. [RTOS 对象与任务关系](task/README.md)：了解初始化和消息传递。
3. [MotorTask](task/Motor_Task/README.md)：了解路径、自检、事务分发。
4. [电机模块](module/motor/README.md)：了解 Emm 应答与电机状态。
5. [UART BSP](bsp/uart/README.md)：了解中断与 DMA 数据流。

## 全部目录

| 层 | 文档 | 作用 |
|---|---|---|
| BSP | [board](bsp/board/README.md) | 板级初始化和定时器回调 |
| BSP | [dwt](bsp/dwt/README.md) | 64 位单调微秒计时 |
| BSP | [uart](bsp/uart/README.md) | 三路 DMA 收发与环形缓冲 |
| BSP | [buzzer](bsp/buzzer/README.md) | PE3 有源蜂鸣器 GPIO |
| module | [motor](module/motor/README.md) | Emm 协议、事务、反馈与故障 |
| module | [buzzer](module/buzzer/README.md) | 有源蜂鸣器通断节奏 |
| task | [Motor_Task](task/Motor_Task/README.md) | 运动路径及命令分发 |
| task | [Service_Task](task/Service_Task/README.md) | 蜂鸣器消息及静音控制 |
| 公共 | [app_rtos](task/README.md) | 两个静态队列与异常钩子 |
| 配置 | [config](config/README.md) | 缓冲、时限、自检参数 |

## 数据流与通用约定

```text
外部任务 → 命令队列 → MotorTask 分发器 ← 非阻塞自检生成器
                           ↓
              路径 state_machine → handle → axis_control_drv
                           ↓
                     module/motor
                           ↓
                     bsp/uart → 电机
电机 → DMA/ISR → 字节缓冲 → module/motor → 反馈与路径到位判定
```

- 外部操作经任务 API；module/motor 与轴对象由 MotorTask 独占，不自行加互斥锁。
- 函数入参 `now`、各 `*_ms` 字段使用 HAL 毫秒；DWT 返回微秒。不要混用单位。
- X/Z：mm、mm/s；Yaw：度、度/s；协议层使用脉冲、整数 RPM、编码器刻度。
- 入队成功、DMA 接受、驱动器 ACK、路径到位是不同阶段，分别检查对应返回值与状态。
- 头文件记录调用约束，C 文件注释解释关键分支；各目录文档包含初始化、使用方法及边界。
- 现有 HAL 桩测试不模拟真实电气、RTOS 调度或驱动器；编译通过不能代替上板验证。

构建与再生成接入说明见 [工程说明](../README_基础架构.md)。
