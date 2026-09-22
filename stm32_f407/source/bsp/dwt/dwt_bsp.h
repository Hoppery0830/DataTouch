/**
 * @file dwt_bsp.h
 * @brief DWT 周期计数的 64 位扩展；提供单调微秒时间，供中断与任务使用。
 * @see 本目录 README.md（职责、调用顺序与使用限制）。
 */
#pragma once
#include <stdint.h>
/** @brief 使能并清零 DWT 周期计数；仅启动时调用，不在每次实验开始时清零。 */
void DWT_BSP_Init(void);
/* Call at least once per CYCCNT wrap (~25.57s at 168MHz); TIM2 does this. */
/** @brief 返回启动后的单调微秒数。任务/普通 ISR 可调用；PRIMASK 短临界区保护 64 位累计值。
 * 至少每次硬件回绕前调用一次（168MHz 下约 25.57s）；假设核时钟固定且为整数 MHz。 */
uint64_t DWT_BSP_NowUs(void);
