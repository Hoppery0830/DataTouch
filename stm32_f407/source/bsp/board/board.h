/**
 * @file board.h
 * @brief 板级初始化与 TIM2 回调桥接；HAL 外设初始化完成后接入。
 * @see 本目录 README.md（职责、调用顺序与使用限制）。
 */
#pragma once
#include "stm32f4xx_hal.h"
/** @brief 初始化单调时基并关闭蜂鸣器；在 main 的 USER CODE 2 中调用一次。 */
void Board_Init(void);
/** @brief 启动 TIM2 更新中断；须在线程句柄和电机对象就绪后调用。失败进入 Error_Handler。 */
void Board_StartControlTick(void);
/** @brief HAL 定时器回调桥接；仅处理 TIM2，维护 DWT 并通知 MotorTask，保留 TIM6 的 HAL 时基逻辑。
 */
void Board_TimerCallback(TIM_HandleTypeDef *timer);
