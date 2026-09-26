# 公共配置

`app_config.h` 集中存放基础常量。机械参数独立在 `task/Motor_Task/motor_config.c`，CubeMX 的引脚、时钟、DMA/NVIC 配置保存在 `.ioc`。

| 宏 | 当前值 | 作用 |
|---|---|---|
| APP_CONTROL_HZ | 500 | 设计控制频率；不会自动改 TIM2 寄存器 |
| APP_MOTOR_COUNT | 3 | 三路固定电机串口对象 |
| APP_MOTOR_ADDRESS | 1 | 每路驱动器地址；独立链路可相同 |
| APP_MOTOR_REPLY_MS | 80 | 单事务等待应答时限 |
| APP_MOTOR_POLL_MS | 20 | 查询调度间隔 |
| APP_MOTOR_STALE_MS | 300 | 位置/状态反馈有效期 |
| APP_MOTOR_BOOT_MS | 600 | 第一次查询前启动等待 |
| APP_UART_DMA_SIZE | 128 | 每路 DMA 接收缓冲字节数 |
| APP_UART_RING_SIZE | 512 | 每路字节环容量 |
| APP_UART_TX_SIZE | 32 | 每路持久化 TX 缓冲容量 |
| APP_BEEP_ACTIVE_HIGH | 1 | 有源蜂鸣器高电平响 |
| APP_MOTOR_SELFTEST | 0 | 旧往复自检关闭 |
| APP_MOTOR_AUTOSTART | 0 | 远程联调禁止上电自动运动 |
| APP_AUTOSTART_MODE | ACTION_PRESS | 默认按压，可选择 RUB/SLIDE 模式 |
| APP_SELFTEST_AXIS | MOTOR_X | 测试 X 轴 |
| APP_SELFTEST_TARGET | 2.0f | X/Z 为 mm，Yaw 为度 |
| APP_SELFTEST_SPEED | 1.0f | X/Z 为 mm/s，Yaw 为度/s |

修改后重新编译。自检开关带 ifndef，可通过编译宏覆盖；本机关闭测试使用 `--selftest-disabled`。

修改电机数量不能仅改一个宏：UART 映射、轴枚举、位图、配置表和任务循环也要同步。修改蜂鸣器极性须同步 CubeMX 初始输出；修改控制频率须同步 TIM2 与相关时序。

默认 X/Z 螺距、Yaw 直驱、方向和限位都是待实机确认的机械假设。自检是会实际运动的测试流程，自检和上电动作都关闭后只保留查询与外部命令响应。
