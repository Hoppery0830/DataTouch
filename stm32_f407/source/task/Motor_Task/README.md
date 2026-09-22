# MotorTask：运动路径与命令调度

本任务按参考 Arm_Task 的职责拆分：路径 state_machine 决定走哪些点，handle 执行当前点，axis_control_drv 适配物理单位；电机待机/使能/位置速度/故障由 module/motor 管理。

| 文件 | 职责 |
|---|---|
| motor_task.c/.h | Strong 任务入口、外部 API、命令分发和状态快照 |
| motor_state_machine.c/.h | 点到点、往复、多点路径校验和切换 |
| motor_handle.c/.h | 当前点的逐轴下发、到位确认和停留 |
| axis_control_drv.c/.h | mm/度与脉冲/RPM/编码器换算 |
| motor_config.c/.h | X/Z/Yaw 参数和默认限制 |
| motor_action.c/.h、motor_action_handle.c | [按压、揉搓、滑动固定动作](ACTIONS.md)的阶段管理与路径构造 |
| motor_startup.c/.h | [上电单次动作](STARTUP.md)：在线、使能、设零和自动提交 |
| motor_selftest.c/.h | [上电自检](SELFTEST.md)，不是独立任务 |

## 主循环顺序

1. 等待 UART、TIM2、命令或停止事件，最长 2 tick。
2. 消费停止标志，取消自检/路径及旧命令；处理三轴接收与通信超时。
3. 根据独立 TIM2 计数推进路径，再推进固定动作阶段；多个节拍被合并时累计 missed_ticks，活动路径中止。
4. 外部命令优先。取出外部命令后取消自动自检；没有待处理命令时推进启动/自检生成器（编译开关互斥）。
5. 相同分发器处理外部/自检命令；电机有未完成事务时最多等待 500ms。
6. 处理自检首次失败，推进空闲查询，发布一致状态快照。

所有运动处理非阻塞，没有每轴一个任务。线程标志可能合并，因此节拍计数与事件标志分开保存。max_iteration_us 包含本轮业务处理但不包含等待时间。

## 外部接口与完成语义

| API | 语义 |
|---|---|
| MotorTask_Submit | 非阻塞入队；true 仅表示入队 |
| MotorTask_RunAction | 提交按压、揉搓或滑动；true 仅表示入队 |
| MotorTask_Stop | 独立停止标志，下一轮清队列并停止路径 |
| MotorTask_GetStatus | 短临界区复制快照，不返回可修改的内部对象 |

命令类型：PATH、ACTION、ENABLE、ORIGIN、CLEAR_FAULT。PATH 用 path，ACTION 用 action_mode，其余用 axis；enable 字段仅 ENABLE 使用。必须在任务上下文且 RTOS 对象就绪后调用。

`last_command_ok` 表示最近一次分发接受，不是 ACK。查看 enabled、origin_valid、online 及 path_status 判断后续状态。online 当前包含“无锁存故障”，不是仅串口有数据；position 可能保留旧值，需要同时看 online。

运行路径或固定动作时冲突的新路径、使能、清零等命令会被拒绝，不自动打断当前路径。要立即中止使用 Stop。自检失败或外部停止后不自动续跑。

## 路径格式

- axis_mask：bit0=X、bit1=Z、bit2=Yaw；只校验和执行选中轴。
- point_count：1–8；cycles 为正整数，一轮按顺序执行全部点。
- POINT_TO_POINT：仅 1 点、1 轮。
- RECIPROCATING：固定 2 点；每轮 A→B，最终停在 B，不自动追加返回 A。
- WAYPOINTS：按点列表顺序执行，下一轮由最后一点直接前往第一点。
- 每点 target 为当前原点下的绝对目标，speed 为正速度幅值。
- dwell_ms 为所有轴到位后的停留；segment_timeout_ms 包含下发等待、运动及停留。

整条路径先预检。每轴在当前点仅发一次位置命令；到位条件为健康、无待确认控制命令（允许后台查询）、位置和状态时间均晚于下发、到位标志有效且误差在容差内。失去到位条件会重启停留计时。

多轴独立下发、各自运动，不保证同时启动、直线插补或同时到达。

## 单位与参数

```text
指令脉冲 = |目标轴位置| / 每电机转轴位移 × 每转脉冲数
RPM = 轴速度 / 每电机转轴位移 × 60
轴反馈 = 有符号编码器刻度 / 65536 × 每电机转轴位移 × 方向符号
```

脉冲和 RPM 四舍五入；过小速度无法用整数 RPM 表示时被拒绝。X/Z 默认 1mm/转、3200 脉冲/转、0–50mm、8mm/s，Yaw 默认直驱 360°/转、±180°、90°/s。具体默认值见 motor_config.c，减速比、方向和加速度参数需与实物核对。

## 使用示例

确认使能、原点和在线状态后，在外部任务提交：

```c
motor_command_t cmd = {.kind = MOTOR_CMD_PATH};
cmd.path.mode = PATH_POINT_TO_POINT;
cmd.path.axis_mask = 1U << MOTOR_X;
cmd.path.point_count = 1;
cmd.path.cycles = 1;
cmd.path.segment_timeout_ms = 10000;
cmd.path.points[0].target[MOTOR_X] = 1.0f;
cmd.path.points[0].speed[MOTOR_X] = 1.0f;
bool queued = MotorTask_Submit(&cmd);
```

此代码不应未经检查就放进启动阶段执行。当前位置清零不是机械寻零；本阶段没有硬件限位输入。完整启动步骤见工程根 README。
