/** @file test_esp_uart.c
 * @brief Verify shared RX callback routing and full-size asynchronous transmissions.
 */
#include "esp_uart.h"
#include "usart.h"
#include "remote_protocol.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
UART_HandleTypeDef huart1, huart3;
static uint8_t *rx_slot, *tx_slot;
static unsigned k230_events, rx_arms;
static uint16_t tx_size;
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *h);
void ESP_UART_TxComplete(UART_HandleTypeDef *h);
void Touch_UART_RxComplete(UART_HandleTypeDef *h)
{
    if (h == &huart1) k230_events++;
}
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *h, uint8_t *p, uint16_t n)
{
    assert(h == &huart3 && n == 1);
    rx_slot = p; rx_arms++;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *h, uint8_t *p, uint16_t n)
{
    assert(h == &huart3);
    tx_slot = p; tx_size = n;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_Abort(UART_HandleTypeDef *h) { (void)h; return HAL_OK; }
int main(void)
{
    assert(ESP_UART_Start());
    HAL_UART_RxCpltCallback(&huart1);
    assert(k230_events == 1 && rx_arms == 1);
    *rx_slot = 0xa5;
    HAL_UART_RxCpltCallback(&huart3);
    uint8_t b;
    assert(rx_arms == 2 && k230_events == 1);
    assert(ESP_UART_Read(&b) && b == 0xa5 && !ESP_UART_Read(&b));
    uint8_t payload[RC_MAX_PAYLOAD] = {0}, frame[RC_MAX_PAYLOAD + 6];
    size_t n = RC_Build(frame, 0x21, payload, 108);
    assert(ESP_UART_Send(frame, n) && tx_size == n);
    assert(!ESP_UART_Send(frame, n));
    assert(!memcmp(frame, tx_slot, n));
    memset(frame, 0xff, n);
    assert(tx_slot[0] == 0xaa); /* HAL retains an owned buffer, not the caller's stack. */
    ESP_UART_TxComplete(&huart1);
    assert(!ESP_UART_Send(frame, n));
    ESP_UART_TxComplete(&huart3);
    n = RC_Build(frame, 0x21, payload, RC_MAX_PAYLOAD);
    assert(ESP_UART_Send(frame, n));
    ESP_UART_TxComplete(&huart3);
    assert(!ESP_UART_Send(frame, sizeof(frame)));
    puts("PASS ESP UART: K230 RX routing, ESP RX rearm, telemetry/max-size TX, busy, buffer ownership");
}
