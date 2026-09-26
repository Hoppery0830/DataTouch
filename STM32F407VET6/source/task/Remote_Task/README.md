# RemoteTask

App_RTOS_Objects_Init 创建静态队列和 3072 字节栈的 Normal 优先级任务。任务每 2 tick 检查 USART3 字节流，解析后向 MotorTask 投命令；STOP/RESET 使用独立优先队列，并取消此前未执行的普通请求。普通队列 8 条、紧急队列 4 条、回复队列 32 条。

MotorTask 独占控制器和三轴对象；RemoteTask 只收发协议、发布链路时刻和读取一致状态快照。待发送的回复在 UART 忙时保留，状态每 250ms 发送；队列满的普通请求回复 busy，回复偶发丢失通过 ESP32 同序号重试获得缓存结果。

CubeMX 接入使用现有 USER CODE 中的 App_RTOS_Objects_Init，USART3 IRQ 同步写入 .ioc。详见 [联调说明](../../../联调说明_小程序_ESP32_STM32.md)。
