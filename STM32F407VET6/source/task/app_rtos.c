/**
 * @file app_rtos.c
 * @brief 集中创建静态 RTOS 队列，并提供栈溢出、分配失败处理钩子。
 * @see 本目录 README.md（职责、调用顺序与使用限制）。
 */
#include "app_rtos.h"
#include "main.h"
#include "remote_task.h"
QueueHandle_t g_motor_commands, g_buzzer_commands;
static StaticQueue_t motor_cb, buzzer_cb;
static uint8_t motor_storage[4 * sizeof(motor_command_t)];
static uint8_t buzzer_storage[4 * sizeof(buzzer_pattern_t)];
/** @brief 创建两个长度为 4 的静态消息队列。内核初始化后、创建任务前调用一次。
 * 队列按值复制消息；创建失败进入 Error_Handler，不使用运行期堆分配。 */
void App_RTOS_Objects_Init(void)
{
    g_motor_commands = xQueueCreateStatic(4, sizeof(motor_command_t), motor_storage, &motor_cb);
    g_buzzer_commands = xQueueCreateStatic(4, sizeof(buzzer_pattern_t), buzzer_storage, &buzzer_cb);
    RemoteTask_Init();
    if (!g_motor_commands || !g_buzzer_commands)
        Error_Handler();
}
/** @brief FreeRTOS 栈溢出钩子；进入统一错误处理，不能在此继续执行任务业务。 */
void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    (void)name;
    Error_Handler();
}
/** @brief FreeRTOS 分配失败钩子；进入统一错误处理。 */
void vApplicationMallocFailedHook(void)
{
    Error_Handler();
}
