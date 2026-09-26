> 当前固件默认上电执行一次按压，旧往复自检关闭。开关：APP_MOTOR_AUTOSTART=1、APP_AUTOSTART_MODE=ACTION_PRESS、APP_MOTOR_SELFTEST=0。详见 source/task/Motor_Task/STARTUP.md（工程根目录下）。

# 自检状态机

实现：motor_selftest.c/.h，由 MotorTask 主循环调用。当前开关为 1，选择 X 轴、目标 2mm、速度 1mm/s。参数来自 app_config.h，关闭开关后仍保留状态接口，但状态始终 DISABLED，不产生命令。

| 状态 | 动作与转移 |
|---|---|
| WAIT_ONLINE | 等被测轴健康，进入 ENABLE |
| ENABLE | 产生使能命令；分发成功后等新状态反馈确认使能 |
| ZERO | 产生清零命令；等原点有效、新位置/状态且位置接近零 |
| PATH | 产生两点路径：目标→零，1 轮；等待 PATH_DONE |
| PAUSE | 累计完成轮数，等待 500ms 后再次 PATH |
| STOPPED | 外部停止/命令取消自动测试，不自动重启 |
| FAILED | 拒绝、故障或超时锁存，不自动清错或重启 |
| DISABLED | 自检关闭 |

## 与任务分发器的约定

`SelfTest_Update` 一次只执行一个步骤，返回 true 时填充一个命令。调用者接收该命令后，先完成正常分发，再调用 `SelfTest_CommandResult`；期间不能重复向自检索取命令。accepted 表示分发结果，不是电机 ACK。

使能要求更新的 flags_ms，清零要求更新的 position_ms/flags_ms 以及原点有效、位置接近零。清零后 position_valid 暂时为 false 是正常阶段，留给轮询更新，不立即判故障。

WAIT_ONLINE、ENABLE、ZERO 各有 5s 上限；等待分发器空闲另受 500ms 规则约束。每个路径点 10s，整轮 25s；回零完成后停 500ms 再重复。

## 停止与可观测状态

SelfTest_Cancel 仅终止自动生成器，不直接发电机停止。MotorTask_Stop 同时终止生成器、清队列并请求电机停止。外部普通命令取出后自动取消自检，但仍受路径正在运行时的冲突拒绝规则约束。

首个 FAILED 由 MotorTask 转为路径中止和三轴停止请求；之后轮询和手动恢复接口仍运行。没有自动重启，也没有清故障后自动恢复自检。

状态快照中的 selftest_state 表示阶段，selftest_rounds 表示真正完成目标→零点的次数。原点是启动位置的坐标零，不是物理限位原点。

测试包含阶段推进、新反馈门控、回零路径、重复间隔、停止/拒绝/故障锁存、计时回绕，以及宏关闭状态。硬件调度和实际动作需上板验证。更多操作见 [测试说明](../../../测试说明_电机自检.md)。
