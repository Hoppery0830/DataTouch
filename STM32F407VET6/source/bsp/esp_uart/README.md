# ESP32 通信 BSP

USART3 PB10 TX / PB11 RX，115200 8N1，IRQ 优先级 6。独立 256 字节 SPSC 接收环、32 字节发送缓冲；中断只搬字节，协议解析由 RemoteTask 完成。

HAL_UART_RxCpltCallback 接收单字节后重新挂接；共享的 TX 完成/错误回调从电机 uart_bsp 转发，按 huart3 筛选，不改变电机 DMA 行为。错误/溢出标记 broken，任务侧 Abort 后恢复；不会重放运动命令。

ESP_UART_Send 复制数据后中断发送，忙时返回 false。此实现用于低带宽控制帧和状态，不承担高频传感器流。
