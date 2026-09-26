# dwt：单调微秒时基

文件：`dwt_bsp.c/.h`。提供从启动开始累积的微秒时间，用于任务耗时统计和后续实验时间戳。

## 原理

`DWT_BSP_Init()` 使能 CYCCNT 并清零。`DWT_BSP_NowUs()` 取当前 32 位周期数，用无符号差值累积到 64 位，再按核频率换算微秒。

```text
delta = uint32_t(now_cycle - last_cycle)
cycles64 += delta
us = cycles64 / (SystemCoreClock / 1000000)
```

168MHz 下硬件约 25.57s 回绕。必须在此之前至少读取一次；当前 TIM2 每 2ms 调用。跨越多次回绕无法从一个 32 位计数恢复丢失时间。

## 并发与限制

累计值和上次计数在短 PRIMASK 临界区内更新，避免任务与普通 ISR 交错产生错误。离开临界区后做除法，减少关中断时间。不应从 NMI 等不可屏蔽上下文调用。

假定核时钟固定、至少 1MHz 且为整数 MHz。调试暂停、低功耗或关 DWT 会影响计数，不能把它视为独立于 CPU 的墙上时钟。

实验开始时只记录 `T0_us=DWT_BSP_NowUs()`，不要再次 Init 清零共享时基。计算耗时采用两次微秒值之差；RTOS 等待和 motor 超时仍使用毫秒。
