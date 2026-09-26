> 当前远程联调固件关闭上电演示；ARM 复用准备流程，START 单独触发动作。下文上电演示配置是此前测试用法，当前请以工程根目录《联调说明_小程序_ESP32_STM32.md》为准。

# 三个固定动作

固定动作在 MotorTask 中非阻塞推进，不新增 FreeRTOS 任务。`motor_action.c` 管理模式和阶段，`motor_action_handle.c` 构造每段路径，复用已有 Motion → handle → axis_control_drv → module/motor。使能、停机和故障仍由电机模块处理。

## 默认动作

起点是动作被任务接受时参与轴的位置快照，不是坐标零点。每次调用只完成一次动作。

| API 模式 | 默认顺序 |
|---|---|
| `ACTION_PRESS` 按压 | Z 相对起点 +1mm → 到位停留 300ms → Z 回起点；X/Yaw 不动 |
| `ACTION_RUB` 揉搓 | Z +1mm → 停留 300ms → Yaw 在起始角 -10°/+10° 间往复 3 轮 → Yaw 回起始角 → Z 回起点 |
| `ACTION_SLIDE` 滑动 | Z +1mm → 停留 300ms → X 相对起点 +10mm → Z 先回起点 → X 回起点 |

X/Z 默认速度 1mm/s，Yaw 10°/s；每个路径点超时 15s（包含等待下发、运动和停留）。揉搓一轮定义为前往负偏角、再前往正偏角，三轮共六个目标，最后另加一次回中。通过两点往复路径的 cycles 实现，不受八点数组限制。各阶段等待真实新反馈确认到位后再切换。

参数集中在 [motor_config.c](motor_config.c) 的 `g_action_config`，结构字段见 [motor_config.h](motor_config.h)。Z 正方向暂定为右移，需按实物确认 `g_axis_config` 中方向、导程、细分和行程。这里是位置控制：Z 保持压入位置，不代表恒力控制。

## 调用与状态

在 RTOS 已初始化的外部任务中，根据需要调用其中一个：

```c
bool queued = MotorTask_RunAction(ACTION_PRESS);
// 揉搓：MotorTask_RunAction(ACTION_RUB);
// 滑动：MotorTask_RunAction(ACTION_SLIDE);
```

`true` 仅表示入队。用 `MotorTask_GetStatus()` 查看 `processed_commands`、`last_command_ok` 判断任务是否接受，再通过 `action_status` 判断 `ACTION_RUNNING / DONE / ABORTED / FAILED`。`action_mode` 标识模式，`action_stage` 给出当前阶段，失败或停止保留中断阶段。`path_status`、point、cycle 只描述当前子路径；某段结束不等于整个动作完成。动作字段保留最近一次接受的固定动作结果，普通路径不会清除此记录。

执行前，参与轴必须在线、无故障、已使能、具有有效原点和新鲜位置反馈，且没有正在运行的运动或待确认的控制命令；后台位置/状态/速度查询允许继续。手动调用固定动作 API 不会自动使能、清零或寻找机械原点；上电准备由独立启动流程负责。普通路径或固定动作运行期间，新运动、使能、清零等冲突命令会被拒绝。

**当前默认上电执行一次按压，旧 X 自检关闭。** 启动流程自动准备所选模式的参与轴，见 [STARTUP.md](STARTUP.md)。手动调用 API 仍需自行确认轴准备完成。需要立即中止使用 `MotorTask_Stop()`；等停止处理完成再提交新命令，避免被停止流程清空队列。

## 边界和停止

开始前预检全部目标，包括后续 Yaw 两侧偏角、X 滑动和返回位置；任一目标越界，整个动作拒绝，Z 不会先压入。每段只向需要移动的轴下发命令，同时持续检查整个动作参与轴，防止 Z 保持期间失联仍继续揉搓或滑动。

`MotorTask_Stop()` 通过独立停止标志中止动作、路径并清队列。参与轴故障、通信失效、段超时或漏控制节拍会终止动作并请求三轴停止，不自动退回或续跑。软件串口停止沿用现有模块机制，失联时不能据此确认实际已经停机。

## 验证

`tests/test_framework.c` 用真实路径执行器和 HAL 桩反馈跑完三种动作，检查非零起点返回、各轴命令次数、300ms 停留推进、揉搓循环、滑动退回顺序、后段目标越界提前拒绝、冲突、停止和保持轴失联。测试不包含真实 RTOS 调度、电机机构或接触效果，实际位移和方向仍需上板验证。
