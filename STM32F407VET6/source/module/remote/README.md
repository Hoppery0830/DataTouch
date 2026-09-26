# 远程控制模块

remote_protocol.c/.h 定义无硬件依赖的帧格式与逐字节 CRC 解析；remote_control.c/.h 由 MotorTask 独占，管理模式、ARM、START、STOP/RESET、心跳失效及最近 16 条结果缓存。

准备复用 motor_startup 的运行期 Startup_Prepare，已有原点保留；动作复用 Action_Start/Update。回执通过回调投静态队列，不直接操作串口。所有 Update 均非阻塞，准备、停止和动作阶段均有时限。ACK 接受与动作完成分开。

完整协议和测试见 [三端联调说明](../../../联调说明_小程序_ESP32_STM32.md)。
