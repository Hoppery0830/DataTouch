> 当前远程联调固件关闭上电演示；ARM 复用准备流程，START 单独触发动作。下文上电演示配置是此前测试用法，当前请以工程根目录《联调说明_小程序_ESP32_STM32.md》为准。

# 上电单次动作

默认配置在 `source/config/app_config.h`：

```c
#define APP_MOTOR_SELFTEST 0
#define APP_MOTOR_AUTOSTART 1
#define APP_AUTOSTART_MODE ACTION_PRESS
#define APP_AUTOSTART_SETUP_MS 5000U
```

烧录后运行或复位，MotorTask 等待参与轴在线，逐轴发送使能命令、等待新的使能状态，设置当前位置为零并等待新的近零位置与状态反馈，最后提交一次指定固定动作。默认只准备 Z 轴，执行 +1mm、停留 300ms、返回起点；X/Yaw 不参与正常动作。设零只是建立本次软件坐标，不是机械寻零。正向是否右移取决于轴配置和安装方向。

`APP_AUTOSTART_MODE` 可改为 `ACTION_RUB` 或 `ACTION_SLIDE`；流程分别自动准备 Z/Yaw 或 X/Z。动作参数仍位于 `motor_config.c` 的 `g_action_config`，详见 [ACTIONS.md](ACTIONS.md)。修改配置后重新编译。设 `APP_MOTOR_AUTOSTART=0` 关闭上电动作；若要恢复旧往复自检，必须先关闭此开关，两套流程同时开启会编译报错。

## 实现与状态

`motor_startup.c/.h` 是非阻塞命令生成器，不创建任务，不直接访问串口。MotorTask 的正常分发器执行命令，并返回接受结果；接受不是 ACK 或到位，准备阶段必须再等待反馈。每个准备阶段最多等待 5 秒，动作运行阶段由已有路径段超时管理。

通过 `MotorTask_GetStatus()` 的 `startup_state` 查看：

| 状态 | 含义 |
|---|---|
| STARTUP_DISABLED | 上电演示关闭 |
| STARTUP_WAIT_ONLINE | 等待所有参与轴在线 |
| STARTUP_ENABLE | 当前轴使能与确认 |
| STARTUP_ZERO | 当前轴设零与确认 |
| STARTUP_ACTION | 正在提交或执行固定动作 |
| STARTUP_DONE | 本次演示完成，不重复 |
| STARTUP_STOPPED | 外部命令接管或软件停止，不自动重启 |
| STARTUP_FAILED | 准备/动作失败，不自动重试 |

动作中的细分阶段通过 `action_stage` 查看。外部命令接管会取消后续自动流程，正在运行的动作仍按普通命令冲突规则处理；立即停止请调用 `MotorTask_Stop()`。停止和首次失败会清队列并请求停止三轴。失败后仍保留通信轮询和外部恢复接口；再次自动演示需复位，或由外部完成准备后手动提交动作。

## 验证

主机测试覆盖按压、揉搓、滑动各自的参与轴准备、新反馈门槛、单次完成、取消、命令拒绝、故障和计时回绕/超时，以及关闭开关后不发命令。HAL 桩测试不模拟真实 RTOS 调度和机械运动。上板前确认起始位置向 Z 正方向至少有配置位移的可用行程；程序不寻找机械边界。
