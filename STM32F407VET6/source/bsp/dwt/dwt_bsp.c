/**
 * @file dwt_bsp.c
 * @brief DWT 周期计数的 64 位扩展；提供单调微秒时间，供中断与任务使用。
 * @see 本目录 README.md（职责、调用顺序与使用限制）。
 */
#include "dwt_bsp.h"
#include "stm32f4xx.h"
static uint32_t last_cycle;
static uint64_t cycles;
/** @brief 使能并清零 DWT 周期计数；仅启动时调用，不在每次实验开始时清零。 */
void DWT_BSP_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    last_cycle = 0;
    cycles = 0;
}
/** @brief 返回启动后的单调微秒数。任务/普通 ISR 可调用；PRIMASK 短临界区保护 64 位累计值。
 * 至少每次硬件回绕前调用一次（168MHz 下约 25.57s）；假设核时钟固定且为整数 MHz。 */
uint64_t DWT_BSP_NowUs(void)
{
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    uint32_t now = DWT->CYCCNT;
    /* 无符号减法自然覆盖一次 32 位回绕；跨越两次回绕则无法补回丢失周期。 */
    cycles += (uint32_t)(now - last_cycle);
    last_cycle = now;
    uint64_t snapshot = cycles;
    __set_PRIMASK(mask);
    return snapshot / (SystemCoreClock / 1000000U);
}
