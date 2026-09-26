#include "k230_protocol.h"

#include "main.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define K230_HEADER_0              0xAAU
#define K230_HEADER_1              0x55U
#define K230_TYPE_LIGHT_CONTROL    0x10U
#define K230_TYPE_U_CURVE          0x20U
#define K230_TYPE_ACK              0x80U
#define K230_STATUS_OK             0x00U
#define K230_MAX_PAYLOAD_LENGTH       4U
#define K230_RX_RING_SIZE            64U
#define K230_ACK_FRAME_LENGTH          7U
#define K230_UART_TX_TIMEOUT_MS       20U

typedef enum
{
  K230_PARSE_HEADER_0 = 0U,
  K230_PARSE_HEADER_1,
  K230_PARSE_TYPE,
  K230_PARSE_LENGTH,
  K230_PARSE_PAYLOAD,
  K230_PARSE_CHECKSUM
} K230_ParseState_t;

static UART_HandleTypeDef *k230_uart = NULL;
static uint8_t uart2_rx_byte = 0U;
static uint8_t rx_ring[K230_RX_RING_SIZE] = {0U};
static volatile uint8_t rx_head = 0U;
static volatile uint8_t rx_tail = 0U;
static volatile uint8_t rx_overflow = 0U;
static volatile uint8_t rx_error = 0U;

static K230_ParseState_t parse_state = K230_PARSE_HEADER_0;
static uint8_t frame_type = 0U;
static uint8_t frame_length = 0U;
static uint8_t frame_payload[K230_MAX_PAYLOAD_LENGTH] = {0U};
static uint8_t frame_payload_index = 0U;
static uint8_t frame_checksum = 0U;

volatile float texture_u_curve = 0.0f;
volatile uint8_t texture_u_curve_valid = 0U;

static void K230_DebugText(const char *text)
{
  Debug_UART1_Write((const uint8_t *)text, (uint16_t)strlen(text));
}

static void K230_ResetParser(void)
{
  parse_state = K230_PARSE_HEADER_0;
  frame_type = 0U;
  frame_length = 0U;
  frame_payload_index = 0U;
  frame_checksum = 0U;
}

static void K230_ResetParserFromByte(uint8_t byte)
{
  K230_ResetParser();
  if (byte == K230_HEADER_0)
  {
    parse_state = K230_PARSE_HEADER_1;
  }
}

static uint8_t K230_PopRxByte(uint8_t *byte)
{
  if (rx_tail == rx_head)
  {
    return 0U;
  }

  *byte = rx_ring[rx_tail];
  rx_tail = (uint8_t)((rx_tail + 1U) % K230_RX_RING_SIZE);
  return 1U;
}

void Light_On(void)
{
  HAL_GPIO_WritePin(LIGHT_CTRL_GPIO_Port, LIGHT_CTRL_Pin, GPIO_PIN_SET);
}

void Light_Off(void)
{
  HAL_GPIO_WritePin(LIGHT_CTRL_GPIO_Port, LIGHT_CTRL_Pin, GPIO_PIN_RESET);
}

void K230_SendAck(uint8_t acked_type, uint8_t status)
{
  uint8_t frame[K230_ACK_FRAME_LENGTH];

  if (k230_uart == NULL)
  {
    return;
  }

  frame[0] = K230_HEADER_0;
  frame[1] = K230_HEADER_1;
  frame[2] = K230_TYPE_ACK;
  frame[3] = 2U;
  frame[4] = acked_type;
  frame[5] = status;
  frame[6] = (uint8_t)(frame[2] + frame[3] + frame[4] + frame[5]);

  (void)HAL_UART_Transmit(
      k230_uart,
      frame,
      (uint16_t)sizeof(frame),
      K230_UART_TX_TIMEOUT_MS);
}

static void K230_PrintUCurve(float value)
{
  char message[64];
  int length = snprintf(
      message,
      sizeof(message),
      "K230 U_curve = %.6f\r\n",
      (double)value);

  if ((length > 0) && (length < (int)sizeof(message)))
  {
    Debug_UART1_Write((const uint8_t *)message, (uint16_t)length);
  }
  else
  {
    K230_DebugText("UART2 FRAME ERROR\r\n");
  }
}

static void K230_HandleFrame(void)
{
  if (frame_type == K230_TYPE_LIGHT_CONTROL)
  {
    if ((frame_length != 1U) ||
        ((frame_payload[0] != 0x00U) && (frame_payload[0] != 0x01U)))
    {
      K230_DebugText("UART2 FRAME ERROR\r\n");
      return;
    }

    if (frame_payload[0] == 0x01U)
    {
      Light_On();
      K230_DebugText("K230 LIGHT_ON\r\n");
    }
    else
    {
      Light_Off();
      K230_DebugText("K230 LIGHT_OFF\r\n");
    }

    K230_SendAck(K230_TYPE_LIGHT_CONTROL, K230_STATUS_OK);
    return;
  }

  if (frame_type == K230_TYPE_U_CURVE)
  {
    uint32_t raw;
    float value;

    if (frame_length != 4U)
    {
      K230_DebugText("UART2 FRAME ERROR\r\n");
      return;
    }

    raw = ((uint32_t)frame_payload[0]) |
          ((uint32_t)frame_payload[1] << 8U) |
          ((uint32_t)frame_payload[2] << 16U) |
          ((uint32_t)frame_payload[3] << 24U);
    memcpy(&value, &raw, sizeof(value));

    if (!isfinite(value))
    {
      K230_DebugText("UART2 FRAME ERROR\r\n");
      return;
    }

    texture_u_curve = value;
    texture_u_curve_valid = 1U;
    K230_PrintUCurve(value);
    K230_SendAck(K230_TYPE_U_CURVE, K230_STATUS_OK);
    return;
  }

  K230_DebugText("UART2 FRAME ERROR\r\n");
}

static void K230_ParseByte(uint8_t byte)
{
  switch (parse_state)
  {
    case K230_PARSE_HEADER_0:
      if (byte == K230_HEADER_0)
      {
        parse_state = K230_PARSE_HEADER_1;
      }
      break;

    case K230_PARSE_HEADER_1:
      if (byte == K230_HEADER_1)
      {
        parse_state = K230_PARSE_TYPE;
      }
      else if (byte != K230_HEADER_0)
      {
        parse_state = K230_PARSE_HEADER_0;
      }
      break;

    case K230_PARSE_TYPE:
      frame_type = byte;
      frame_checksum = byte;
      parse_state = K230_PARSE_LENGTH;
      break;

    case K230_PARSE_LENGTH:
      frame_length = byte;
      frame_checksum = (uint8_t)(frame_checksum + byte);
      frame_payload_index = 0U;
      if (frame_length > K230_MAX_PAYLOAD_LENGTH)
      {
        K230_DebugText("UART2 FRAME ERROR\r\n");
        K230_ResetParserFromByte(byte);
      }
      else if (frame_length == 0U)
      {
        parse_state = K230_PARSE_CHECKSUM;
      }
      else
      {
        parse_state = K230_PARSE_PAYLOAD;
      }
      break;

    case K230_PARSE_PAYLOAD:
      frame_payload[frame_payload_index] = byte;
      ++frame_payload_index;
      frame_checksum = (uint8_t)(frame_checksum + byte);
      if (frame_payload_index >= frame_length)
      {
        parse_state = K230_PARSE_CHECKSUM;
      }
      break;

    case K230_PARSE_CHECKSUM:
      if (byte == frame_checksum)
      {
        K230_HandleFrame();
        K230_ResetParser();
      }
      else
      {
        K230_DebugText("UART2 CHECKSUM ERROR\r\n");
        K230_ResetParserFromByte(byte);
      }
      break;

    default:
      K230_DebugText("UART2 FRAME ERROR\r\n");
      K230_ResetParserFromByte(byte);
      break;
  }
}

void K230_Protocol_Init(UART_HandleTypeDef *uart)
{
  k230_uart = uart;
  rx_head = 0U;
  rx_tail = 0U;
  rx_overflow = 0U;
  rx_error = 0U;
  texture_u_curve = 0.0f;
  texture_u_curve_valid = 0U;
  K230_ResetParser();
  Light_Off();

  if ((k230_uart == NULL) ||
      (HAL_UART_Receive_IT(k230_uart, &uart2_rx_byte, 1U) != HAL_OK))
  {
    Error_Handler();
  }
}

void K230_Protocol_RxByte(uint8_t byte)
{
  uint8_t next_head = (uint8_t)((rx_head + 1U) % K230_RX_RING_SIZE);

  if (next_head == rx_tail)
  {
    rx_overflow = 1U;
    return;
  }

  rx_ring[rx_head] = byte;
  rx_head = next_head;
}

void K230_Protocol_Process(void)
{
  uint8_t byte;

  if ((rx_overflow != 0U) || (rx_error != 0U))
  {
    rx_overflow = 0U;
    rx_error = 0U;
    rx_tail = rx_head;
    K230_ResetParser();
    K230_DebugText("UART2 FRAME ERROR\r\n");
  }

  while (K230_PopRxByte(&byte) != 0U)
  {
    K230_ParseByte(byte);
  }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if ((k230_uart != NULL) && (huart->Instance == k230_uart->Instance))
  {
    K230_Protocol_RxByte(uart2_rx_byte);
    if (HAL_UART_Receive_IT(k230_uart, &uart2_rx_byte, 1U) != HAL_OK)
    {
      rx_error = 1U;
    }
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if ((k230_uart != NULL) && (huart->Instance == k230_uart->Instance))
  {
    rx_error = 1U;
    (void)HAL_UART_Receive_IT(k230_uart, &uart2_rx_byte, 1U);
  }
}
