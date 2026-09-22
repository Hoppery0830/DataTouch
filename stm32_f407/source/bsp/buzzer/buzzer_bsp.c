/**
 * @file buzzer_bsp.c
 * @brief PE3 有源蜂鸣器 GPIO 驱动；将逻辑开关转换成配置的有效电平。
 * @see 本目录 README.md（职责、调用顺序与使用限制）。
 */
#include "buzzer_bsp.h"
#include "main.h"
#include "app_config.h"
/** @brief 设置有源蜂鸣器逻辑开关；on=true 发声。GPIO 必须已初始化，有效电平取自 app_config.h。 */
void Buzzer_BSP_Set(bool on)
{
    HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin,
                      (on == (APP_BEEP_ACTIVE_HIGH != 0)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
