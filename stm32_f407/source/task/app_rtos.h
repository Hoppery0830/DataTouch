/**
 * @file app_rtos.h
 * @brief 集中创建静态 RTOS 队列，并提供栈溢出、分配失败处理钩子。
 * @see 本目录 README.md（职责、调用顺序与使用限制）。
 */
#pragma once
#include "FreeRTOS.h"
#include "queue.h"
#include "motor_task.h"
#include "service_task.h"
extern QueueHandle_t g_motor_commands, g_buzzer_commands;
/** @brief 创建两个长度为 4 的静态消息队列。内核初始化后、创建任务前调用一次。
 * 队列按值复制消息；创建失败进入 Error_Handler，不使用运行期堆分配。 */
void App_RTOS_Objects_Init(void);
