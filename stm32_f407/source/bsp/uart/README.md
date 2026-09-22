# uart：电机串口 DMA 与字节缓冲

文件：`uart_bsp.c/.h`。管理三路独立收发资源，协议解析在 module/motor 中完成。

| 数组下标 | 电机 | UART | TX / RX |
|---|---|---|---|
| 0 | X | UART5 | PC12 / PD2 |
| 1 | Z | USART6 | PC6 / PC7 |
| 2 | Yaw | USART2 | PD5 / PD6 |

均为 115200、8N1；RX DMA Circular，TX DMA Normal。UART/DMA ISR 抢占优先级统一为 6。

## 对象与接口

每个 `uart_port_t` 持有 128B DMA 接收区、512B 字节环、32B TX 区、head/tail、dma_pos、tx_busy、broken 及错误计数。对象生命周期覆盖 DMA 操作，缓冲位于 DMA 可访问 SRAM，不能放入 F407 CCMRAM。

| 接口 | 说明 |
|---|---|
| UART_BSP_Start | 绑定已完成 HAL 初始化的串口并启动 RX；只初始化一次 |
| UART_BSP_Send | 复制到对象 TX 缓冲再启动 DMA；true 表示接受发送 |
| UART_BSP_Read | 任务侧取出最多指定数量字节，返回实际数量 |
| UART_BSP_Recover | HAL Abort、清缓冲、重启 RX；不清上层故障 |

## 接收与发送流程

IDLE/HT/TC 回调读取当前 NDTR，而非直接采用可能滞后的事件 Size；从 dma_pos 搬运新增字节，更新 head，最后通知 MotorTask。事件边界不是帧边界。任务取字节后让 Emm 解析器组帧。

只允许一个任务消费者更新 tail。UART 与 DMA IRQ 同优先级，串行更新生产端；发布索引前使用 DMB 保证数据可见性。增加其他生产者或修改 IRQ 抢占关系时须重新评估并发模型，不能只依赖 volatile。

发送进行中禁止覆盖 TX。完成回调释放 tx_busy，收到完成回调并不意味着电机 ACK。错误回调仅记录故障并通知任务，恢复操作在任务上下文完成。

## 边界

环形缓冲满会累计 dropped 并标 broken，不静默覆盖；dropped 是溢出事件计数，不是精确丢失帧数。恢复时清除所有残留字节，并交由 module 决定故障处理。

循环 DMA 需要及时服务中断；128B、115200 下半缓冲时间约 5.56ms。若屏蔽中断超过整圈，单靠 NDTR 无法知道圈数。HAL Abort 为同步操作，虽没有 RTOS 等待，仍不适合放进 ISR。

新增 UART 回调应在这里统一分发，避免多个强定义冲突。未知句柄当前直接忽略。验证包含回绕、重复事件、忙发送、溢出和恢复，真实中断时序需上板测量。

K230 摄像头预留 USART1（STM32 PA9 TX / PA10 RX），当前仅初始化串口，尚未接入摄像头协议。Motor_Yaw 使用 USART2，RX 为 DMA1 Stream5 Channel4 循环接收，TX 为 DMA1 Stream6 Channel4 普通发送；CubeMX 配置和中断已同步。
