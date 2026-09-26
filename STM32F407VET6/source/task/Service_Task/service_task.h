/**
 * @file service_task.h
 * @brief 低优先级服务任务；消费蜂鸣器队列并处理取消与静音请求。
 * @see 本目录 README.md（职责、调用顺序与使用限制）。
 */
#pragma once
#include "buzzer.h"
/** @brief 任务侧将提示音按值放入队列；true 仅代表入队，静音时可能被服务任务丢弃。 */
bool ServiceTask_Beep(buzzer_pattern_t pattern);
/** @brief 任务侧设置静音请求；在下一次服务循环生效，置 true 同时请求取消当前和排队节奏。 */
void ServiceTask_SetMuted(bool muted);
/** @brief 任务侧请求取消所有提示音；与普通队列独立，下一次 ServiceTask 循环处理。 */
void ServiceTask_CancelBeep(void);
/** @brief 覆盖 CubeMX Weak 入口；单独持有蜂鸣器对象，每 5 个 tick 处理控制标志、队列和节奏。
 * 当前 RTOS 为 1000Hz，因此周期约 5ms；不是用阻塞延时播放整段提示音。 */
void StartServiceTask(void *argument);
