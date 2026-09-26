#ifndef K230_PROTOCOL_H
#define K230_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

extern volatile float texture_u_curve;
extern volatile uint8_t texture_u_curve_valid;

void K230_Protocol_Init(UART_HandleTypeDef *uart);
void K230_Protocol_RxByte(uint8_t byte);
void K230_Protocol_Process(void);
void K230_SendAck(uint8_t acked_type, uint8_t status);

void Light_On(void);
void Light_Off(void);

#ifdef __cplusplus
}
#endif

#endif /* K230_PROTOCOL_H */
