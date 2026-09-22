#pragma once
#include <stdint.h>
typedef enum
{
    HAL_OK = 0,
    HAL_ERROR = 1
} HAL_StatusTypeDef;
typedef struct
{
    uint32_t counter;
} DMA_HandleTypeDef;
typedef struct
{
    DMA_HandleTypeDef *hdmarx;
} UART_HandleTypeDef;
#define __DMB() ((void)0)
#define __disable_irq() ((void)0)
#define __get_PRIMASK() 0U
#define __set_PRIMASK(x) ((void)(x))
#define __HAL_DMA_GET_COUNTER(d) ((d)->counter)
HAL_StatusTypeDef HAL_UARTEx_ReceiveToIdle_DMA(UART_HandleTypeDef *, uint8_t *, uint16_t);
HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef *, uint8_t *, uint16_t);
HAL_StatusTypeDef HAL_UART_Abort(UART_HandleTypeDef *);
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *, uint16_t);
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *);
void HAL_UART_ErrorCallback(UART_HandleTypeDef *);
