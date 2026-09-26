/**
 * @file service_task.c
 * @brief 低优先级服务任务；消费蜂鸣器队列并处理取消与静音请求。
 * @see 本目录 README.md（职责、调用顺序与使用限制）。
 */
#include "service_task.h"
#include "app_rtos.h"
#include "cmsis_os2.h"
#include "task.h"
static volatile bool requested_mute, cancel;
/** @brief 任务侧将提示音按值放入队列；true 仅代表入队，静音时可能被服务任务丢弃。 */
bool ServiceTask_Beep(buzzer_pattern_t p)
{
    if (!g_buzzer_commands || !p.repeat || !p.on_ms || p.on_ms > 60000 || p.off_ms > 60000)
        return false;
    return xQueueSend(g_buzzer_commands, &p, 0) == pdPASS;
}
/** @brief 任务侧设置静音请求；在下一次服务循环生效，置 true 同时请求取消当前和排队节奏。 */
void ServiceTask_SetMuted(bool mute)
{
    taskENTER_CRITICAL();
    requested_mute = mute;
    if (mute)
        cancel = true;
    taskEXIT_CRITICAL();
}
/** @brief 任务侧请求取消所有提示音；与普通队列独立，下一次 ServiceTask 循环处理。 */
void ServiceTask_CancelBeep(void)
{
    taskENTER_CRITICAL();
    cancel = true;
    taskEXIT_CRITICAL();
}
/** @brief 覆盖 CubeMX Weak 入口；单独持有蜂鸣器对象，每 5 个 tick 处理控制标志、队列和节奏。
 * 当前 RTOS 为 1000Hz，因此周期约 5ms；不是用阻塞延时播放整段提示音。 */
void StartServiceTask(void *argument)
{
    (void)argument;
    buzzer_t b;
    Buzzer_Init(&b);
    for (;;)
    {
        bool muted, clear;
        taskENTER_CRITICAL();
        muted = requested_mute;
        clear = cancel;
        cancel = false;
        taskEXIT_CRITICAL();
        Buzzer_Mute(&b, muted);
        if (clear)
        {
            xQueueReset(g_buzzer_commands);
            Buzzer_Cancel(&b);
        }
        buzzer_pattern_t p;
        if (muted)
        {
            while (xQueueReceive(g_buzzer_commands, &p, 0) == pdPASS)
            {
            }
        }
        else if (!b.active && xQueueReceive(g_buzzer_commands, &p, 0) == pdPASS)
            (void)Buzzer_Play(&b, &p, HAL_GetTick());
        Buzzer_Update(&b, HAL_GetTick());
        osDelay(5);
    }
}
