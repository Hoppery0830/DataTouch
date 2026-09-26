/**
 * @file uart_bsp.c
 * @brief 三路电机串口 DMA 收发与字节缓冲；ISR 生产数据，MotorTask 消费。
 * @see 本目录 README.md（职责、调用顺序与使用限制）。
 */
#include "uart_bsp.h"
#include <string.h>
/* Other UART transports share HAL callbacks without sharing motor DMA buffers. */
__attribute__((weak)) void ESP_UART_TxComplete(UART_HandleTypeDef *h){(void)h;}
__attribute__((weak)) void ESP_UART_Error(UART_HandleTypeDef *h){(void)h;}
uart_port_t g_motor_ports[APP_MOTOR_COUNT];
/** @brief 按 HAL 句柄查找持久化串口对象；未知串口返回 NULL，不接管其他外设回调。 */
static uart_port_t *find(UART_HandleTypeDef *h)
{
    for (unsigned i = 0; i < APP_MOTOR_COUNT; i++)
        if (g_motor_ports[i].hal == h)
            return &g_motor_ports[i];
    return NULL;
}
/** @brief 绑定持久化串口对象并开启循环 RX DMA；MotorTask 初始化时调用，返回是否启动成功。
 * p/hal 必须有效，HAL 的 RX/TX DMA 已配置；不能对正在收发的对象再次初始化。 */
bool UART_BSP_Start(uart_port_t *p, UART_HandleTypeDef *hal)
{
    memset(p, 0, sizeof(*p));
    p->hal = hal;
    p->broken = HAL_UARTEx_ReceiveToIdle_DMA(hal, p->dma, sizeof(p->dma)) != HAL_OK;
    return !p->broken;
}
/** @brief 尝试发送一条报文；返回 true 仅表示 HAL 已接受 DMA 发送，不代表电机确认。
 * 数据复制到对象自有 TX 缓冲，忙/故障/长度不合法时返回 false；仅由 MotorTask 调用。 */
bool UART_BSP_Send(uart_port_t *p, const uint8_t *data, size_t size)
{
    if (!data || !size || size > sizeof(p->tx) || p->broken || p->tx_busy)
        return false;
    memcpy(p->tx, data, size);
    p->tx_busy = true;
    if (HAL_UART_Transmit_DMA(p->hal, p->tx, (uint16_t)size) != HAL_OK)
    {
        p->tx_busy = false;
        p->broken = true;
        p->errors++;
        return false;
    }
    return true;
}
/** @brief 从字节环形缓冲取最多 capacity 字节，返回实际字节数；不是按协议帧读取。
 * 仅一个任务消费者，out 必须能容纳 capacity 字节；帧拼接交给协议层。 */
size_t UART_BSP_Read(uart_port_t *p, uint8_t *out, size_t capacity)
{
    /* SPSC: only MotorTask changes tail; same-priority UART/DMA IRQs change head. */
    uint32_t tail = p->tail, head = p->head;
    __DMB();
    size_t n = 0;
    while (tail != head && n < capacity)
        out[n++] = p->ring[tail++ % APP_UART_RING_SIZE];
    __DMB();
    p->tail = tail;
    return n;
}
/** @brief 任务侧中止收发、丢弃残留字节并重启 RX DMA；返回恢复是否成功。
 * 使用 HAL 同步 Abort，不能在 ISR 调用；恢复通信不会清电机故障或重发运动命令。 */
bool UART_BSP_Recover(uart_port_t *p)
{
    /* HAL blocking abort disables DMA/UART callbacks; no RTOS wait here. */
    if (HAL_UART_Abort(p->hal) != HAL_OK)
        return false;
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    p->head = p->tail = 0;
    p->dma_pos = 0;
    p->tx_busy = false;
    p->broken = false;
    __set_PRIMASK(mask);
    p->broken = HAL_UARTEx_ReceiveToIdle_DMA(p->hal, p->dma, sizeof(p->dma)) != HAL_OK;
    return !p->broken;
}
/** @brief UART/DMA 中断入口：根据当前 NDTR 提取新增字节并通知任务；不解析协议。
 * 同一端口 UART 和 DMA IRQ 配成相同抢占优先级，防止两个生产者并发修改 head。 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *h, uint16_t size)
{
    (void)size;
    uart_port_t *p = find(h);
    if (!p || p->broken)
        return;
    /* Read current NDTR, not a stale HT Size when IDLE and HT IRQs are pending. */
    uint16_t pos = (uint16_t)(APP_UART_DMA_SIZE - __HAL_DMA_GET_COUNTER(h->hdmarx));
    if (pos == APP_UART_DMA_SIZE)
        pos = 0;
    while (p->dma_pos != pos)
    {
        if ((uint32_t)(p->head - p->tail) >= APP_UART_RING_SIZE)
        {
            p->dropped++;
            p->broken = true;
            break;
        }
        p->ring[p->head % APP_UART_RING_SIZE] = p->dma[p->dma_pos];
        __DMB();
        p->head++;
        p->dma_pos = (uint16_t)((p->dma_pos + 1U) % APP_UART_DMA_SIZE);
    }
    UART_BSP_NotifyFromISR();
}
/** @brief 发送完成 ISR：释放对象 TX 缓冲使用权；这是串口发送完成，不是电机执行完成。 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *h)
{
    ESP_UART_TxComplete(h);
    uart_port_t *p = find(h);
    if (p)
    {
        p->tx_busy = false;
        UART_BSP_NotifyFromISR();
    }
}
/** @brief 错误 ISR：记录错误并标记端口待恢复；实际 Abort/重启由任务执行。 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *h)
{
    ESP_UART_Error(h);
    uart_port_t *p = find(h);
    if (p)
    {
        p->errors++;
        p->broken = true;
        UART_BSP_NotifyFromISR();
    }
}
