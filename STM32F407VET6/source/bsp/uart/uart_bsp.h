/**
 * @file uart_bsp.h
 * @brief 三路电机串口 DMA 收发与字节缓冲；ISR 生产数据，MotorTask 消费。
 * @see 本目录 README.md（职责、调用顺序与使用限制）。
 */
#pragma once
#include "stm32f4xx_hal.h"
#include "app_config.h"
#include <stdbool.h>
#include <stddef.h>
typedef struct
{
    UART_HandleTypeDef *hal;
    uint8_t dma[APP_UART_DMA_SIZE], ring[APP_UART_RING_SIZE], tx[APP_UART_TX_SIZE];
    volatile uint32_t head, tail, errors, dropped;
    volatile bool tx_busy, broken;
    uint16_t dma_pos; /**< DMA 环中已收取的位置，IDLE/HT/TC 共用，避免重复搬运。 */
} uart_port_t;
extern uart_port_t g_motor_ports[APP_MOTOR_COUNT];
/** @brief 绑定持久化串口对象并开启循环 RX DMA；MotorTask 初始化时调用，返回是否启动成功。
 * p/hal 必须有效，HAL 的 RX/TX DMA 已配置；不能对正在收发的对象再次初始化。 */
bool UART_BSP_Start(uart_port_t *p, UART_HandleTypeDef *hal);
/** @brief 尝试发送一条报文；返回 true 仅表示 HAL 已接受 DMA 发送，不代表电机确认。
 * 数据复制到对象自有 TX 缓冲，忙/故障/长度不合法时返回 false；仅由 MotorTask 调用。 */
bool UART_BSP_Send(uart_port_t *p, const uint8_t *data, size_t size);
/** @brief 从字节环形缓冲取最多 capacity 字节，返回实际字节数；不是按协议帧读取。
 * 仅一个任务消费者，out 必须能容纳 capacity 字节；帧拼接交给协议层。 */
size_t UART_BSP_Read(uart_port_t *p, uint8_t *out, size_t capacity);
/* MotorTask only; abort/restart a faulty port, never retransmit a command. */
/** @brief 任务侧中止收发、丢弃残留字节并重启 RX DMA；返回恢复是否成功。
 * 使用 HAL 同步 Abort，不能在 ISR 调用；恢复通信不会清电机故障或重发运动命令。 */
bool UART_BSP_Recover(uart_port_t *p);
/** @brief 串口 ISR 唤醒桥接：置线程标志 EVENT_RX；中断优先级须满足 FreeRTOS ISR API 约束。 */
void UART_BSP_NotifyFromISR(void); /* supplied by task layer */
