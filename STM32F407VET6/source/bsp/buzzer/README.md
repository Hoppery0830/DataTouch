# buzzer BSP：有源蜂鸣器 GPIO

文件：`buzzer_bsp.c/.h`，硬件为 PE3 / BEEP。`Buzzer_BSP_Set(bool on)` 把逻辑开关转换为 GPIO 电平，不产生 PWM，也不管理时长。

当前 `APP_BEEP_ACTIVE_HIGH=1`：true 输出高电平发声，false 输出低电平关闭。CubeMX GPIO 初始低电平与它一致；实际驱动若低电平有效，需要同时修改此宏及 CubeMX 初始电平。

调用前须完成 `MX_GPIO_Init()`。Board_Init 在调度前关断一次；进入运行阶段后由 ServiceTask 的蜂鸣器模块统一控制，其他任务经 ServiceTask API 请求提示音，避免互相覆盖电平。

有源蜂鸣器的音高由内部振荡电路决定，软件只控制通断节奏。不能用此接口替代无源蜂鸣器 PWM 驱动。上层节奏见 [module/buzzer](../../module/buzzer/README.md)。
