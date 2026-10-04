#include "touch_task.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os2.h"
#include <stdio.h>
#include <string.h>

static StaticTask_t task_cb;
static StackType_t task_stack[1024];
volatile touch_status_t g_touch_status;
static touch_status_t current;
static touch_parser_t parser;
static uint32_t sent_force, sent_adc, last_output, last_status;

void TouchTask_GetStatus(touch_status_t *out)
{
    if (!out) return;
    taskENTER_CRITICAL(); *out = g_touch_status; taskEXIT_CRITICAL();
}
/* F/A/S 的字段、单位兼容用户原 Python 上位机。输出只取最新值，不回放积压数据。 */
static void output_pc(uint32_t now)
{
    if (now - last_output < TOUCH_OUTPUT_MS) return;
    last_output = now;
    char text[384];
    size_t used = 0;
    int n;
    bool force = current.force_online && current.force.count != sent_force;
    bool adc = current.adc_online && current.adc_blocks != sent_adc;
    bool status = now - last_status >= 1000U;
    if (force) {
        const touch_force_t *f = &current.force;
        n = snprintf(text + used, sizeof(text) - used,
            "F,%lu,%ld,%ld,%ld,%ld,%ld,%ld,%s,%lu\r\n",
            (unsigned long)f->timestamp_ms,
            (long)Touch_Milli(f->values[0] - TOUCH_ZERO_FX_N),
            (long)Touch_Milli(f->values[1] - TOUCH_ZERO_FY_N),
            (long)Touch_Milli(f->values[2] - TOUCH_ZERO_FZ_N),
            (long)Touch_Milli(f->values[3]), (long)Touch_Milli(f->values[4]),
            (long)Touch_Milli(f->values[5]),
            f->mode == TOUCH_FORCE_ACTIVE ? "ACTIVE" : "MODBUS", (unsigned long)f->count);
        if (n < 0 || (size_t)n >= sizeof(text) - used) return;
        used += (size_t)n;
    }
    if (adc) {
        n = snprintf(text + used, sizeof(text) - used, "A,%lu,%ld,%ld,%ld,%lu,%lu\r\n",
            (unsigned long)current.adc_timestamp_ms,
            (long)current.adxl.mg[0], (long)current.adxl.mg[1], (long)current.adxl.mg[2],
            (unsigned long)current.adxl.rms_mg, (unsigned long)current.adc_blocks);
        if (n < 0 || (size_t)n >= sizeof(text) - used) return;
        used += (size_t)n;
    }
    if (status) {
        const touch_bsp_stats_t *s = &current.hardware;
        n = snprintf(text + used, sizeof(text) - used, "S,%lu,%lu,%lu,%lu,%lu,%lu,%lu\r\n",
            (unsigned long)now, (unsigned long)current.force.count,
            (unsigned long)current.crc_errors, (unsigned long)s->uart_errors,
            (unsigned long)s->ring_overflows, (unsigned long)s->adc_overruns,
            (unsigned long)s->adc_errors);
        if (n < 0 || (size_t)n >= sizeof(text) - used) return;
        used += (size_t)n;
    }
    if (Touch_BSP_SendPC(text, used)) {
        if (force) sent_force = current.force.count;
        if (adc) sent_adc = current.adc_blocks;
        if (status) last_status = now;
    }
}
static void run(void *argument)
{
    (void)argument;
    if (!Touch_BSP_Start()) Error_Handler();
    uint32_t start = HAL_GetTick(), last_poll = start;
    touch_adc_block_t block;
    for (;;) {
        uint32_t now = HAL_GetTick();
        if (Touch_BSP_Recover(now)) {
            parser.used = 0;
            parser.latest.valid = false;
        }
        uint8_t byte;
        /* 有界工作量，不让坏帧/连续发送占满 CPU；每毫秒可消费512字节。 */
        for (unsigned i = 0; i < 512 && Touch_BSP_ReadByte(&byte); i++)
            (void)Touch_Parse(&parser, byte, now);
        for (unsigned i = 0; i < 4 && Touch_BSP_ReadBlock(&block); i++) {
            if (Touch_ProcessADC(block.samples, TOUCH_BLOCK_FRAMES, &current.adxl)) {
                current.adc_timestamp_ms = block.timestamp_ms;
                current.adc_blocks = block.sequence;
            }
        }
        now = HAL_GetTick(); /* DMA 回调可能在本轮处理期间发布更晚的块时间戳。 */
        current.force = parser.latest;
        current.force_online = parser.latest.valid && now - parser.latest.timestamp_ms < TOUCH_STALE_MS;
        current.adc_online = current.adc_blocks != 0 && now - current.adc_timestamp_ms < TOUCH_STALE_MS;
        current.crc_errors = parser.crc_errors;
        current.invalid_values = parser.invalid_values;
        Touch_BSP_GetStats(&current.hardware);
        /* 先被动监听 2s；没有新鲜主动帧才询问 Modbus，掉线后也可重新发现。 */
        if (now - start >= 2000U && now - last_poll >= 10U &&
            !(current.force_online && current.force.mode == TOUCH_FORCE_ACTIVE)) {
            if (Touch_BSP_RequestForce()) last_poll = now;
        }
        taskENTER_CRITICAL(); g_touch_status = current; taskEXIT_CRITICAL();
        output_pc(now);
        osDelay(1);
    }
}
void TouchTask_Init(void)
{
    const osThreadAttr_t attr = {
        .name = "TouchTask", .cb_mem = &task_cb, .cb_size = sizeof(task_cb),
        .stack_mem = task_stack, .stack_size = sizeof(task_stack),
        .priority = osPriorityNormal1 /* 低于电机 AboveNormal1，高于远程 Normal。 */
    };
    if (!osThreadNew(run, NULL, &attr)) Error_Handler();
}
