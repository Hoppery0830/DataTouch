# 触觉 BSP

PC0 接地仍读4095时，可展开 `g_touch_adc_debug` 检查实际寄存器快照。
预期 `pc_mode=[3,3,3]`、`pc_pull=[0,0,0]`、`channels=[10,11,12]`、
`sequence_length=3`、`sampling_code=[6,6,6]`、`resolution_bits=12`、
`left_aligned=0`、`adc_clock_hz=21000000`；DMA 的两个地址应分别等于 expected 对应字段。
`dma_first` 是最近任务消费的完整块前两组 XYZ，未经标定和均值处理；不是读取 ADC DR 寄存器得到，
不会与 DMA 争读。配置快照每100ms刷新，原始块字段每处理一块更新，运行中的多字段读取不是同一瞬间快照。
这只能验证软件可见配置和数据，不能证明实际 PC0 电压、VDDA/VREF+ 供电或板上走线正常。

串口无数据排查：在 Ozone 展开 `g_touch_io_debug`，运行数秒后暂停。
`pc_tx_started/pc_tx_completed` 分别是电脑 DMA 发送接受/USART TC 完成次数，
`force_rx_bytes` 是 UART4 DMA 搬入接收环的累计字节数，`force_queries` 是成功提交的查询次数。
`first_rx[0:first_rx_size]` 保存启动后的前32字节，可按十六进制检查主动帧头 `53 54` 或回复头 `01 03 18`。
无字节且 `force_dma_remaining` 一直512表示尚未观察到 UART4 DMA 接收；有字节但有效帧为0则结合 CRC 错误和原始数据查协议/波特率。
100ms 刷新的 GPIO 配置预期为 PA9 mode=2/AF=7，PC10/PC11 mode=2/AF=8；当前时钟下 pc_brr=729、force_brr=91。
该对象可绕过调试器不能求值 GPIO 地址强制转换表达式的问题；发送完成仍不等于电脑已收到，配置正确后需验证实际引脚波形和板上通路。

`Touch_BSP_Start()` 由 TouchTask 首次运行时调用一次，晚于全部 MX 初始化。独占 ADC1（PC0/1/2）、TIM3、UART4（六维力）和 USART1（电脑输出）。完整参数及 DMA 分配见 [触觉整合说明](../../../README_触觉整合.md)。

UART4 用 512 字节循环 DMA 配合 IDLE/HT/TC 中断，将增量数据搬入 2048 字节单生产者/单消费者环形缓冲。UART4 与 RX DMA IRQ 均为优先级 6，以当前 NDTR 计算新增位置。`Touch_BSP_ReadByte` 只由 TouchTask 调用。溢出标记断流，在任务侧 Abort、清缓存并重启，通知协议层丢弃半帧。

ADC 双缓冲为 600 个 uint16_t；每半区 100 组 XYZ。ISR 复制完成半区到 4 元素块队列，携带 HAL 毫秒及累计块序号。块队列的 head 由 ISR 写，tail 由任务写，用 DMB 发布；满时丢新块计数。中断只复制，不算 RMS、不打印。

USART1 的发送数据复制至独立 384 字节缓冲，DMA 发送，UART TC 回调释放缓冲。忙时返回 false 并计数，不等待。UART4 查询用静态 8 字节缓冲的中断发送，不控制 DE，不发送传感器硬件清零命令。

UART HAL 回调仍定义在 `bsp/uart/uart_bsp.c`，转发至 `Touch_UART_*`；这些钩子的弱实现用于原电机主机测试，实机链接本 BSP 强实现。ADC 回调及 UART4/USART1/ADC/DMA1_Stream2/DMA2_Stream0/DMA2_Stream7 IRQ 定义在本文件。不要在其他源文件重复定义。

运行期错误由 `Touch_BSP_Recover` 每 100ms 最多恢复一次。传感器没有回包不视为硬件初始化失败；任务用数据新鲜度判断离线。总计数可通过 `Touch_BSP_GetStats` 获取；ADC 中断超过一个完整 DMA 周期的极端停顿无法仅凭 NDTR 精确重建漏采量，长时间断点调试的数据不作为连续采样依据。
