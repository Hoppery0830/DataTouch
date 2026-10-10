# TouchTask

`App_RTOS_Objects_Init()` 调用 `TouchTask_Init()`，使用静态 TCB 和 1024 Words 栈创建唯一任务，优先级 Normal1；低于 MotorTask 的 AboveNormal1，高于 RemoteTask 的 Normal。无需 CubeMX 新增任务。

任务先初始化触觉 BSP，然后每 1 tick（当前 1ms）处理最多 512 字节和 4 个 ADC 块，检查数据新鲜度、推进 Modbus 查询、发布快照及尝试发送电脑文本。所有循环有界，USART1 输出走 DMA，避免阻塞采样和电机调度。采样上电持续运行，不受 ARM/START/STOP 控制，也不触发电机动作。

外部任务通过 `TouchTask_GetStatus()` 取临界区保护快照；调试器可查看 `g_touch_status`。`force.values` 为未扣零偏的 N/N·m，电脑 F 行为扣除预置力零偏后的 mN/mN·m。`adc_blocks` 是 DMA 块序号，不是电脑已发送行数；50Hz 输出通常隔一个 100Hz 块选择最新结果。

`force_online/adc_online` 基于 300ms 新鲜度，另有通信、丢块和无效数值计数；不要仅凭旧的 `force.valid` 判断当前在线。初始化配置失败进入现有 Error_Handler；普通运行期采样/通信错误独立恢复，不清除电机故障、不自动重发运动命令。

完整接线、串口格式、标定和上板检查见 [整合说明](../../../README_触觉整合.md)。
