/**
 * @file buzzer_bsp.h
 * @brief PE3 有源蜂鸣器 GPIO 驱动；将逻辑开关转换成配置的有效电平。
 * @see 本目录 README.md（职责、调用顺序与使用限制）。
 */
#pragma once
#include <stdbool.h>
/** @brief 设置有源蜂鸣器逻辑开关；on=true 发声。GPIO 必须已初始化，有效电平取自 app_config.h。 */
void Buzzer_BSP_Set(bool on);
