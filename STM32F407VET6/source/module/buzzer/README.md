# buzzer：非阻塞通断节奏

文件：`buzzer.c/.h`。只管理一个当前节奏，由 ServiceTask 独占。队列、静音请求的跨任务传递属于 ServiceTask。

`buzzer_pattern_t`：on_ms 为每次发声时长，off_ms 为两次发声间隔，repeat 为次数。on_ms 必须为 1–60000ms，off_ms 为 0–60000ms，repeat 至少 1。

```text
Play → ON →（on_ms 到期）OFF
            ├─ 剩余次数为 0：结束
            └─ 等待 off_ms → ON → …
```

| 接口 | 行为 |
|---|---|
| Buzzer_Init | 清状态并关 GPIO |
| Buzzer_Play | 检查参数与静音；开始或替换当前节奏，返回是否成功 |
| Buzzer_Update | 按 now 毫秒推进至多一个阶段，不阻塞 |
| Buzzer_Cancel | 停止当前节奏，保留静音属性 |
| Buzzer_Mute | 置静音并取消当前节奏；解除静音不续播 |

最后一次 ON 结束后立即完成，不额外计最后一个 off_ms。到期判断使用有符号时间差覆盖 32 位时钟回绕，时限必须小于半个计数范围。

Update 以实际调用时刻安排下一阶段，任务延迟可能拉长节奏，不会补发错过的快速开关。ServiceTask 每约 5ms 调用，off_ms=0 也可能至少间隔一次服务调度。不同实例不应同时控制同一蜂鸣器。

外部任务优先使用 [ServiceTask_Beep](../../task/Service_Task/README.md)，不要直接调用此对象。单元测试覆盖次数、静音取消和时间回绕。
