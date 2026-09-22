/** @brief USART3 PB10/PB11，115200 8N1，中断接收环与中断发送。 */
#pragma once
#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stddef.h>
bool ESP_UART_Start(void);
bool ESP_UART_Read(uint8_t *byte);
bool ESP_UART_Send(const uint8_t *data, size_t n);
void ESP_UART_Recover(void);
