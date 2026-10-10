#include "remote_task.h"
#include "motor_task.h"
#include "esp_uart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "cmsis_os2.h"
#include "main.h"
static QueueHandle_t requests, urgent, replies;
static StaticQueue_t req_cb, urgent_cb, reply_cb;
static uint8_t req_mem[8 * sizeof(remote_request_t)], urgent_mem[4 * sizeof(remote_request_t)],
    reply_mem[32 * sizeof(remote_reply_t)];
static StaticTask_t thread_cb;
static StackType_t stack[768];
static volatile uint32_t last_rx;
static volatile bool connected;
bool RemoteTask_LinkAlive(uint32_t now)
{
    return connected && now - last_rx < 2000U;
}
bool RemoteTask_Take(remote_request_t *q, bool priority)
{
    return xQueueReceive(priority ? urgent : requests, q, 0) == pdPASS;
}
void RemoteTask_Reply(remote_reply_t q)
{
    (void)xQueueSend(replies, &q, 0);
}
static void run(void *argument)
{
    (void)argument;
    if (!ESP_UART_Start())
        Error_Handler();
    remote_parser_t parser = {0};
    uint32_t last_status = 0;
    remote_reply_t reply;
    bool pending = false;
    for (;;)
    {
        uint32_t now = HAL_GetTick();
        ESP_UART_Recover();
        uint8_t b;
        for (unsigned budget = 0; budget < 128 && ESP_UART_Read(&b); budget++)
            if (RC_Parse(&parser, b, now))
            {
                uint8_t type = parser.bytes[2], len = parser.bytes[3];
                const uint8_t *p = parser.bytes + 4;
                if (type == RC_HEARTBEAT && len == 0)
                {
                    last_rx = now;
                    connected = true;
                }
                if (type == RC_REQUEST && len == 6)
                {
                    last_rx = now;
                    connected = true;
                    remote_request_t q = {RC_Read32(p), p[4], p[5]};
                    if (q.op == RC_STOP || q.op == RC_RESET)
                    {
                        /* Drop preceding ordinary commands, with explicit cancellation receipts. */
                        remote_request_t old;
                        while (xQueueReceive(requests, &old, 0) == pdPASS)
                            RemoteTask_Reply((remote_reply_t){old.seq, RC_CANCELLED});
                        if (xQueueSend(urgent, &q, 0) != pdPASS)
                        {
                            MotorTask_Stop();
                            RemoteTask_Reply((remote_reply_t){q.seq, RC_BUSY});
                        }
                    }
                    else if (xQueueSend(requests, &q, 0) != pdPASS)
                        RemoteTask_Reply((remote_reply_t){q.seq, RC_BUSY});
                }
            }
        uint8_t frame[32], payload[5];
        if (!pending)
            pending = xQueueReceive(replies, &reply, 0) == pdPASS;
        if (pending)
        {
            RC_Write32(payload, reply.seq);
            payload[4] = reply.result;
            size_t n = RC_Build(frame, RC_REPLY, payload, 5);
            if (ESP_UART_Send(frame, n))
                pending = false;
        }
        else if (now - last_status >= 250)
        {
            motor_task_status_t s;
            MotorTask_GetStatus(&s);
            payload[0] = s.remote.state;
            payload[1] = s.remote.mode;
            payload[2] = s.remote.stage;
            payload[3] = s.remote.online;
            payload[4] = s.remote.fault;
            size_t n = RC_Build(frame, RC_STATUS, payload, 5);
            if (ESP_UART_Send(frame, n))
                last_status = now;
        }
        osDelay(2);
    }
}
void RemoteTask_Init(void)
{
    requests = xQueueCreateStatic(8, sizeof(remote_request_t), req_mem, &req_cb);
    urgent = xQueueCreateStatic(4, sizeof(remote_request_t), urgent_mem, &urgent_cb);
    replies = xQueueCreateStatic(32, sizeof(remote_reply_t), reply_mem, &reply_cb);
    if (!requests || !urgent || !replies)
        Error_Handler();
    const osThreadAttr_t attr = {.name = "RemoteTask",
                                 .cb_mem = &thread_cb,
                                 .cb_size = sizeof(thread_cb),
                                 .stack_mem = stack,
                                 .stack_size = sizeof(stack),
                                 .priority = osPriorityNormal};
    if (!osThreadNew(run, NULL, &attr))
        Error_Handler();
}
