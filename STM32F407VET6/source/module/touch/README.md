# 触觉协议与计算

`touch_processing.c/.h` 不依赖 HAL/FreeRTOS，协议对象由 TouchTask 独占，可以在电脑端 GCC 测试。

`Touch_Parse` 接收一个字节及 HAL 毫秒，支持六维力主动帧和地址 1 的 Modbus 读回复，验证 Modbus CRC16、小端 float32、NaN/Inf 和半帧超时。合法帧更新六个原始物理量、接收时间、模式和帧数；不在模块层扣零偏。CRC 错误逐字节重同步，非法浮点帧整体丢弃且单独计数。状态保留最后有效值，是否新鲜由任务判断。

`Touch_ProcessADC` 接收最多 100 组交错 XYZ。三轴 ADC 均值用整数除法截断，与用户源代码一致；按 `touch_config.h` 六位置标定值计算 mg。振动 RMS 保留原示例的三轴平方偏差求和、除帧数、使用平均灵敏度 416.78 raw/g 换算并向下取整。它不是各轴 RMS 的平均，也不包含块均值中的重力。ADC 达到 0/4095 时标记 clipped，超过 12 位范围拒绝计算。

`Touch_Milli` 转换 N→mN、N·m→mN·m、g→mg，饱和处理超大值，避免浮点转整型溢出。输出量化精度沿用源工程。

本模块未计算综合舒适度评分。原 Python 上位机仍负责摩擦系数、窗口特征和动态曲线；若后续移植，应先定义法向轴、低法向力屏蔽阈值、滤波与窗口，不能直接将当前均值当完整高频波形。

测试：`python tests/run_touch_tests.py --cc <host-gcc>`，涵盖标准 CRC 向量、主动/Modbus 帧、分包、坏帧恢复、非有限数、计数回绕、标定和 RMS/饱和检测。
