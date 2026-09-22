# ServiceTask：蜂鸣器服务

文件：service_task.c/.h。低优先级、静态 2048B 栈，CubeMX Weak 入口由这里的 StartServiceTask 覆盖。任务独占一个 buzzer_t，运行时只由它驱动蜂鸣器模块。

## 接口

```c
ServiceTask_Beep((buzzer_pattern_t){100, 100, 2}); // 响100ms、间隔100ms、2次
ServiceTask_SetMuted(true);                     // 下次循环取消并静音
ServiceTask_SetMuted(false);                    // 允许新提示，不恢复旧提示
ServiceTask_CancelBeep();                       // 取消当前及排队提示
```

接口仅供任务上下文调用。Beep 返回 true 是进入队列，不保证播放；队列容量为 4，满或参数非法返回 false。静音期间仍可能入队成功，但消费端会丢弃它。

SetMuted/Cancel 使用独立标志及短临界区，不依赖队列有空位。它们在下一轮生效，不是调用瞬间就改变 GPIO；正常周期约 5ms，也可能受高优先级任务影响。

## 循环顺序

1. 一次性读取静音/取消请求。
2. 设置模块静音；有取消请求则清队列并取消当前节奏。
3. 静音时清除待播消息，否则只有当前空闲才取下一条。
4. 调用 Buzzer_Update，再 osDelay(5)。

使用 osDelay 的只是服务循环间隔；完整音序由非阻塞状态机维护。取消与队列并发时，此前已成功入队的提示也可能被清除，这是取消全部提示的语义。

启动只关闭蜂鸣器，不默认发声。采集业务接入时主动调用静音接口；当前没有自动判断实验阶段。模块细节见 [buzzer](../../module/buzzer/README.md)。
