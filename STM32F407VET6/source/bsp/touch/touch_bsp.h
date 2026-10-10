/** @file touch_bsp.h
 * @brief 触觉专用外设：TIM3/ADC1、UART4 RX、USART1 PC TX；TouchTask 单消费者。
 */
#pragma once
#include "stm32f4xx_hal.h"
#include "touch_config.h"
#include <stdbool.h>
#include <stddef.h>
typedef struct {
    uint16_t samples[TOUCH_BLOCK_FRAMES * 3];
    uint32_t timestamp_ms, sequence;
} touch_adc_block_t;
typedef struct {
    uint32_t uart_errors, ring_overflows, adc_overruns, adc_errors;
    uint32_t pc_errors, pc_drops;
} touch_bsp_stats_t;
/* Ozone 展开此对象即可查看实际外设配置；计数自本次上电累计，不因链路恢复清零。
 * 调试快照不是业务同步接口。first_rx 保存最先收到的最多32字节，避免高速滚动难以观察。 */
typedef struct {
    uint32_t snapshot_ms;
    uint32_t pc_tx_started, pc_tx_completed, force_rx_bytes, force_queries;
    uint32_t force_last_error, pc_last_error;
    uint32_t pc_brr, pc_cr1, pc_dma_remaining;
    uint32_t force_brr, force_cr1, force_dma_remaining;
    uint32_t pa9_mode, pa9_af, pc10_mode, pc10_af, pc11_mode, pc11_af;
    uint32_t first_rx_size;
    uint8_t first_rx[32];
} touch_io_debug_t;
extern volatile touch_io_debug_t g_touch_io_debug;
/* ADC 硬件只读快照，避免 Ozone 对 GPIO 地址表达式报 out of scope。
 * dma_first 是任务取到的最后完整块前两组XYZ，不读实时DR、不干扰DMA。 */
typedef struct {
    uint32_t snapshot_ms, block_sequence;
    uint32_t pc_mode[3], pc_pull[3];
    uint32_t channels[3], sampling_code[3], sequence_length;
    uint32_t resolution_bits, left_aligned, adc_clock_hz;
    uint32_t cr1, cr2, common_ccr, dma_cr, dma_remaining;
    uint32_t dma_peripheral_address, dma_memory_address;
    uint32_t expected_peripheral_address, expected_memory_address;
    uint16_t dma_first[6];
} touch_adc_debug_t;
extern volatile touch_adc_debug_t g_touch_adc_debug;
bool Touch_BSP_Start(void);
/* 任务侧恢复硬件；返回 true 表示传感器字节流已被截断，需清除协议半帧。 */
bool Touch_BSP_Recover(uint32_t now);
bool Touch_BSP_ReadByte(uint8_t *byte);
bool Touch_BSP_ReadBlock(touch_adc_block_t *block);
bool Touch_BSP_RequestForce(void);
bool Touch_BSP_SendPC(const char *data, size_t size);
void Touch_BSP_GetStats(touch_bsp_stats_t *out);
/* 由统一 UART HAL 回调转发，不自行覆盖现有电机回调。 */
void Touch_UART_RxEvent(UART_HandleTypeDef *h);
void Touch_UART_TxComplete(UART_HandleTypeDef *h);
void Touch_UART_Error(UART_HandleTypeDef *h);
