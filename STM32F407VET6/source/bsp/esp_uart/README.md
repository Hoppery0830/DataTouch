# ESP32 通信 BSP

USART3 PB10 TX / PB11 RX，115200 8N1，IRQ 优先级 6。独立 256 字节 SPSC 接收环、133 字节发送缓冲（128 字节负载 + 5 字节协议开销）；中断只搬字节，协议解析由 RemoteTask 完成。

HAL_UART_RxCpltCallback 先向 Touch_UART_RxComplete 转发 USART1/K230 接收事件，再处理 USART3 接收单字节并重新挂接；共享的 TX 完成/错误回调从电机 uart_bsp 转发，按 huart3 筛选，不改变电机 DMA 行为。错误/溢出标记 broken，任务侧 Abort 后恢复；不会重放运动命令。

ESP_UART_Send 复制数据后中断发送，忙时返回 false。发送支持控制帧、状态和降采样后的遥测帧，不直接发送高频传感器原始流。
