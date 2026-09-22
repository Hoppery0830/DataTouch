#include "esp_uart.h"
#include "usart.h"
#include <string.h>
static uint8_t rx, ring[256], tx[32];
static volatile uint32_t head, tail;
static volatile bool broken, busy;
bool ESP_UART_Start(void)
{
    head = tail = 0;
    broken = false;
    busy = false;
    return HAL_UART_Receive_IT(&huart3, &rx, 1) == HAL_OK;
}
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *h)
{
    if (h != &huart3)
        return;
    if (head - tail >= sizeof(ring))
        broken = true;
    else
    {
        ring[head % sizeof(ring)] = rx;
        __DMB();
        head++;
    }
    if (!broken && HAL_UART_Receive_IT(h, &rx, 1) != HAL_OK)
        broken = true;
}
void ESP_UART_TxComplete(UART_HandleTypeDef *h)
{
    if (h == &huart3)
        busy = false;
}
void ESP_UART_Error(UART_HandleTypeDef *h)
{
    if (h == &huart3)
        broken = true;
}
bool ESP_UART_Read(uint8_t *b)
{
    if (broken || head == tail)
        return false;
    __DMB();
    *b = ring[tail % sizeof(ring)];
    __DMB();
    tail++;
    return true;
}
bool ESP_UART_Send(const uint8_t *data, size_t n)
{
    if (broken || busy || n > sizeof(tx))
        return false;
    memcpy(tx, data, n);
    busy = true;
    if (HAL_UART_Transmit_IT(&huart3, tx, (uint16_t)n) != HAL_OK)
    {
        busy = false;
        broken = true;
        return false;
    }
    return true;
}
void ESP_UART_Recover(void)
{
    if (broken)
    {
        HAL_UART_Abort(&huart3);
        (void)ESP_UART_Start();
    }
}
