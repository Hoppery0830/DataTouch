/**
 * @file motor_task.c
 * @brief 电机任务入口、命令分发和状态快照；统一推进通信、路径与自检。
 * @see 本目录 README.md（职责、调用顺序与使用限制）。
 */
#include "motor_task.h"
#include "motor_selftest.h"
#include "motor_startup.h"
#include "remote_control.h"
#include "remote_task.h"
#include "app_rtos.h"
#include "cmsis_os2.h"
#include "board.h"
#include "dwt_bsp.h"
#include "usart.h"
#include <string.h>
extern osThreadId_t MotorTaskHandle;
#define EVENT_RX 1U
#define EVENT_TICK 2U
#define EVENT_COMMAND 4U
#define EVENT_STOP 8U
static motor_t motors[AXIS_COUNT];
static motion_state_t motion;
static motor_action_t action;
static remote_control_t remote;
static motor_task_status_t published;
static volatile uint32_t tick_count;
static volatile bool stop_request;
/** @brief 串口 ISR 唤醒桥接：置线程标志 EVENT_RX；中断优先级须满足 FreeRTOS ISR API 约束。 */
void UART_BSP_NotifyFromISR(void)
{
    if (MotorTaskHandle)
        (void)osThreadFlagsSet(MotorTaskHandle, EVENT_RX);
}
/** @brief TIM2 ISR 累加节拍计数并唤醒 MotorTask；计数用于发现被合并的通知和漏节拍。 */
void MotorTask_TickFromISR(void)
{
    tick_count++;
    if (MotorTaskHandle)
        (void)osThreadFlagsSet(MotorTaskHandle, EVENT_TICK);
}
/** @brief 外部任务非阻塞提交命令；true 只代表入队，false 表示参数非法或队列满。
 * 不可从 ISR 调用。队列按值复制；外部命令被取出后退出自动自检。 */
bool MotorTask_Submit(const motor_command_t *c)
{
    if (!c || !g_motor_commands || c->kind > MOTOR_CMD_ACTION || c->kind < MOTOR_CMD_PATH)
        return false;
    if (c->kind == MOTOR_CMD_PATH)
    {
        if (!Motion_Validate(&c->path))
            return false;
    }
    else if (c->kind == MOTOR_CMD_ACTION)
    {
        if ((unsigned)c->action_mode >= ACTION_MODE_COUNT)
            return false;
    }
    else if ((unsigned)c->axis >= AXIS_COUNT)
        return false;
    if (xQueueSend(g_motor_commands, c, 0) != pdPASS)
        return false;
    if (MotorTaskHandle)
        (void)osThreadFlagsSet(MotorTaskHandle, EVENT_COMMAND);
    return true;
}
/** @brief 外部任务选择固定动作，不自动使能或清零。 */
bool MotorTask_RunAction(motor_action_mode_t mode)
{
    motor_command_t cmd = {.kind = MOTOR_CMD_ACTION, .action_mode = mode};
    return MotorTask_Submit(&cmd);
}
/** @brief 任务上下文发出独立停止请求；下一次 MotorTask 循环中清队列并中止自检/路径。
 * 不受命令队列满限制；这是软件串口停止，不能保证失联驱动器已停机。 */
void MotorTask_Stop(void)
{
    taskENTER_CRITICAL();
    stop_request = true;
    taskEXIT_CRITICAL();
    if (MotorTaskHandle)
        (void)osThreadFlagsSet(MotorTaskHandle, EVENT_STOP);
}
/** @brief 在短临界区复制一致状态快照；任务上下文使用，out=NULL 时不操作。 */
void MotorTask_GetStatus(motor_task_status_t *out)
{
    if (!out)
        return;
    taskENTER_CRITICAL();
    *out = published;
    taskEXIT_CRITICAL();
}
/** @brief 从任务独占对象构造状态，再在短临界区发布，避免外部读取到混合时刻的数据。 */
static void publish(motor_task_status_t *s, uint32_t now)
{
    for (unsigned i = 0; i < AXIS_COUNT; i++)
    {
        s->position[i] = Axis_Position(&g_axis_config[i], motors[i].position);
        s->fault[i] = motors[i].fault;
        s->online[i] = Motor_Healthy(&motors[i], now);
        s->enabled[i] = motors[i].enable_requested && (motors[i].flags & 1U);
        s->origin_valid[i] = motors[i].origin_valid;
        s->stop_failed[i] = motors[i].stop_failed;
    }
    s->remote = remote.status;
    s->path_status = motion.status;
    s->action_mode = action.mode;
    s->action_status = action.status;
    s->action_stage = action.stage;
    s->point = motion.point;
    s->cycle = motion.cycle;
    taskENTER_CRITICAL();
    published = *s;
    taskEXIT_CRITICAL();
}
/** @brief 覆盖 CubeMX Weak 入口：初始化三路通信，然后持续推进通信、路径、自检和命令。
 * 等待事件时阻塞；不会在某个模式中另建无限循环，电机对象仅归本任务所有。 */
void StartMotorTask(void *argument)
{
    (void)argument;
    /* 硬件编号固定映射：UART5→X、USART6→Z、USART2→Yaw。 */
    UART_HandleTypeDef *hal[AXIS_COUNT] = {&huart5, &huart6, &huart2};
    for (unsigned i = 0; i < AXIS_COUNT; i++)
    {
        if (!UART_BSP_Start(&g_motor_ports[i], hal[i]))
            Error_Handler();
        Motor_Init(&motors[i], &g_motor_ports[i], APP_MOTOR_ADDRESS, HAL_GetTick());
    }
    Remote_Init(&remote, RemoteTask_Reply);
    Board_StartControlTick();
    motor_selftest_t selftest;
    SelfTest_Init(&selftest, HAL_GetTick());
    motor_startup_t startup;
    Startup_Init(&startup, HAL_GetTick());
    bool startup_command = false;
    bool selftest_command = false;
    motor_task_status_t status = {0};
    motor_command_t command;
    bool have_command = false;
    uint32_t command_ms = 0, last_tick = 0;
    for (;;)
    {
        (void)osThreadFlagsWait(EVENT_RX | EVENT_TICK | EVENT_COMMAND | EVENT_STOP, osFlagsWaitAny,
                                2);
        uint64_t begin = DWT_BSP_NowUs();
        uint32_t now = HAL_GetTick();
        bool stop;
        taskENTER_CRITICAL();
        stop = stop_request;
        stop_request = false;
        taskEXIT_CRITICAL();
        /* 第一阶段：停止优先于普通命令；随后处理三路反馈与通信超时。 */
        path_status_t previous_path = motion.status;
        if (stop)
        {
            Remote_Cancel(&remote, motors, &motion, &action, now);
            Startup_Cancel(&startup);
            startup_command = false;
            SelfTest_Cancel(&selftest);
            Action_Cancel(&action);
            selftest_command = false;
            Motion_Abort(&motion, motors, false);
            xQueueReset(g_motor_commands);
            have_command = false;
        }
        for (unsigned i = 0; i < AXIS_COUNT; i++)
            Motor_Process(&motors[i], now);
        remote_request_t request;
        bool urgent_remote = false;
        while (RemoteTask_Take(&request, true))
        {
            urgent_remote = true;
            Startup_Cancel(&startup);
            SelfTest_Cancel(&selftest);
            have_command = startup_command = selftest_command = false;
            xQueueReset(g_motor_commands);
            Remote_Handle(&remote, request, motors, &motion, &action, now);
        }
        if (!stop && !urgent_remote && RemoteTask_Take(&request, false))
            Remote_Handle(&remote, request, motors, &motion, &action, now);
        /* 第二阶段：线程标志会合并，独立计数器用来发现漏掉的控制节拍。 */
        uint32_t ticks = tick_count, delta = ticks - last_tick;
        if (delta)
        {
            if (delta > 1)
            {
                status.missed_ticks += delta - 1;
                if (motion.status == PATH_RUNNING)
                    Motion_Abort(&motion, motors, true);
            }
            last_tick = ticks;
            status.control_ticks = ticks;
            Motion_Update(&motion, motors, now);
            Action_Update(&action, &motion, motors, now);
        }
        if (motion.status == PATH_FAILED && previous_path != PATH_FAILED)
        {
            xQueueReset(g_motor_commands);
            have_command = false;
            Startup_CommandResult(&startup, false, now);
            startup_command = false;
            SelfTest_CommandResult(&selftest, false, now);
            selftest_command = false;
        }
        Remote_Update(&remote, motors, &motion, &action, now, RemoteTask_LinkAlive(now));
        /* 第三阶段：外部队列优先，自检仅在分发器空闲时产生下一条命令。 */
        if (!stop && !remote.active && remote.status.state != RC_STOPPING && !have_command &&
            xQueueReceive(g_motor_commands, &command, 0) == pdPASS)
        {
            /* An external command takes ownership; never restart the bench test. */
            Startup_Cancel(&startup);
            startup_command = false;
            SelfTest_Cancel(&selftest);
            selftest_command = false;
            have_command = true;
            command_ms = now;
        }
        if (!stop && !have_command && Startup_Update(&startup, motors, &action, now, &command))
        {
            have_command = true;
            startup_command = true;
            command_ms = now;
        }
        if (!stop && !have_command && SelfTest_Update(&selftest, motors, &motion, now, &command))
        {
            have_command = true;
            selftest_command = true;
            command_ms = now;
        }
        /* 同一个分发器处理外部和自检命令；等待当前事务结束最多 500ms。 */
        if (have_command)
        {
            bool done = true, ok = false;
            if (command.kind == MOTOR_CMD_ACTION)
                ok = Action_Start(&action, command.action_mode, &motion, motors, now);
            else if (command.kind == MOTOR_CMD_PATH && action.status != ACTION_RUNNING)
                ok = Motion_Start(&motion, &command.path, motors, now);
            else if (motion.status != PATH_RUNNING && action.status != ACTION_RUNNING)
            {
                motor_t *m = &motors[command.axis];
                if (m->pending || m->port->tx_busy || m->stop_requested)
                    done = now - command_ms >= 500U;
                else
                    switch (command.kind)
                    {
                    case MOTOR_CMD_ENABLE:
                        ok = Motor_Enable(m, command.enable, now);
                        break;
                    case MOTOR_CMD_ORIGIN:
                        ok = Motor_Zero(m, now);
                        break;
                    case MOTOR_CMD_CLEAR_FAULT:
                        ok = Motor_ClearFault(m, now);
                        break;
                    default:
                        break;
                    }
            }
            if (done)
            {
                if (startup_command)
                    Startup_CommandResult(&startup, ok, now);
                startup_command = false;
                if (selftest_command)
                    SelfTest_CommandResult(&selftest, ok, now);
                selftest_command = false;
                status.processed_commands++;
                status.last_command_ok = ok;
                if (!ok)
                    status.rejected_commands++;
                have_command = false;
            }
        }
        /* 自检首次失败时只触发一次中止；后续仍允许轮询和外部故障恢复命令。 */
        if (selftest.state == SELFTEST_FAILED && status.selftest_state != SELFTEST_FAILED)
        {
            Motion_Abort(&motion, motors, true);
            xQueueReset(g_motor_commands);
            have_command = false;
            selftest_command = false;
        }
        /* 上电准备失败只请求一次停止，锁存失败后仍允许外部恢复。 */
        if (startup.state == STARTUP_FAILED && status.startup_state != STARTUP_FAILED)
        {
            Action_Cancel(&action);
            Motion_Abort(&motion, motors, true);
            xQueueReset(g_motor_commands);
            have_command = false;
            startup_command = false;
        }
        status.startup_state = startup.state;
        status.selftest_state = selftest.state;
        status.selftest_rounds = selftest.rounds;
        for (unsigned i = 0; i < AXIS_COUNT; i++)
            Motor_Poll(&motors[i], now);
        uint32_t elapsed = (uint32_t)(DWT_BSP_NowUs() - begin);
        if (elapsed > status.max_iteration_us)
            status.max_iteration_us = elapsed;
        /* 最后发布快照；统计中的 max_iteration_us 不包含上方事件等待时间。 */
        publish(&status, now);
    }
}
