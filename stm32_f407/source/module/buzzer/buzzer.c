/**
 * @file buzzer.c
 * @brief 有源蜂鸣器非阻塞节奏状态机；由 ServiceTask 持有并周期推进。
 * @see 本目录 README.md（职责、调用顺序与使用限制）。
 */
#include "buzzer.h"
#include "buzzer_bsp.h"
#include <string.h>
/** @brief 初始化节奏对象并关断 GPIO；由 ServiceTask 启动时调用一次。 */
void Buzzer_Init(buzzer_t *b)
{
    memset(b, 0, sizeof(*b));
    Buzzer_BSP_Set(false);
}
/** @brief 取消当前节奏并立即关闭蜂鸣器；仅操作模块对象，不清 RTOS 队列。 */
void Buzzer_Cancel(buzzer_t *b)
{
    b->active = b->on = false;
    b->remaining = 0;
    Buzzer_BSP_Set(false);
}
/** @brief 设置模块静音；置 true 会取消当前节奏，解除静音不恢复旧节奏。 */
void Buzzer_Mute(buzzer_t *b, bool mute)
{
    b->muted = mute;
    if (mute)
        Buzzer_Cancel(b);
}
/** @brief 启动/替换当前节奏；p 包含响/停毫秒数及次数，now 为毫秒。
 * 静音、零次数或时间超范围时返回 false；ServiceTask 仅在空闲时调用它。 */
bool Buzzer_Play(buzzer_t *b, const buzzer_pattern_t *p, uint32_t now)
{
    if (!p || b->muted || !p->repeat || !p->on_ms || p->on_ms > 60000 || p->off_ms > 60000)
        return false;
    b->pattern = *p;
    b->remaining = p->repeat;
    b->active = b->on = true;
    b->deadline = now + p->on_ms;
    Buzzer_BSP_Set(true);
    return true;
}
/** @brief 非阻塞推进一个响/停阶段；到期才切换，最后一次响完即结束，不追加末尾静默阶段。 */
void Buzzer_Update(buzzer_t *b, uint32_t now)
{
    if (!b->active || (int32_t)(now - b->deadline) < 0)
        return;
    if (b->on)
    {
        Buzzer_BSP_Set(false);
        b->on = false;
        if (--b->remaining == 0)
        {
            b->active = false;
            return;
        }
        b->deadline = now + b->pattern.off_ms;
    }
    else
    {
        b->on = true;
        Buzzer_BSP_Set(true);
        b->deadline = now + b->pattern.on_ms;
    }
}
