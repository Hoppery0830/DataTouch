#include "touch_bsp.h"
#include "adc.h"
#include "usart.h"
#include <string.h>

#define RX_DMA_SIZE 512U
#define RX_RING_SIZE 2048U
#define BLOCK_QUEUE_SIZE 4U
static DMA_HandleTypeDef adc_dma, force_dma, pc_dma;
static TIM_HandleTypeDef sample_timer;
static uint16_t adc_samples[TOUCH_BLOCK_FRAMES * 3 * 2];
static uint8_t rx_dma[RX_DMA_SIZE], rx_ring[RX_RING_SIZE];
static volatile uint32_t rx_head, rx_tail, block_head, block_tail;
static uint16_t rx_pos;
static touch_adc_block_t blocks[BLOCK_QUEUE_SIZE];
static uint32_t sequence;
static volatile bool force_broken, adc_broken, pc_broken, pc_busy;
static volatile touch_bsp_stats_t stats;
volatile touch_io_debug_t g_touch_io_debug;
volatile touch_adc_debug_t g_touch_adc_debug;
static uint8_t pc_tx[384];
/* 静态发送缓冲，HAL_UART_Transmit_IT 返回后仍有效；自动换向 RS485 模块。 */
static uint8_t request_all[] = {1, 3, 0, 3, 0, 12, 0xb5, 0xcf};

static bool configure_dma(DMA_HandleTypeDef *d, DMA_Stream_TypeDef *instance,
                          uint32_t channel, uint32_t direction, uint32_t align, uint32_t mode)
{
    d->Instance = instance;
    d->Init.Channel = channel;
    d->Init.Direction = direction;
    d->Init.PeriphInc = DMA_PINC_DISABLE;
    d->Init.MemInc = DMA_MINC_ENABLE;
    d->Init.PeriphDataAlignment = align;
    d->Init.MemDataAlignment = align == DMA_PDATAALIGN_HALFWORD ? DMA_MDATAALIGN_HALFWORD : DMA_MDATAALIGN_BYTE;
    d->Init.Mode = mode;
    d->Init.Priority = DMA_PRIORITY_HIGH;
    d->Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    return HAL_DMA_Init(d) == HAL_OK;
}
static void irq(IRQn_Type n)
{
    HAL_NVIC_SetPriority(n, 6, 0);
    HAL_NVIC_EnableIRQ(n);
}
static bool start_adc(void)
{
    __HAL_TIM_SET_COUNTER(&sample_timer, 0);
    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_samples, TOUCH_BLOCK_FRAMES * 6) != HAL_OK)
        return false;
    return HAL_TIM_Base_Start(&sample_timer) == HAL_OK;
}
bool Touch_BSP_Start(void)
{
    /* 在所有 MX_* 初始化之后调用。板型、时钟树和电机 TIM2 保持原工程配置。
       自有 DMA/IRQ 不放入 CubeMX 生成区，见本目录 README 的资源所有权约束。 */
    __HAL_RCC_DMA1_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();
    huart4.Init.BaudRate = TOUCH_SENSOR_BAUD;
    huart1.Init.BaudRate = TOUCH_PC_BAUD;
    if (HAL_UART_Init(&huart4) != HAL_OK || HAL_UART_Init(&huart1) != HAL_OK) return false;
    if (!configure_dma(&force_dma, DMA1_Stream2, DMA_CHANNEL_4, DMA_PERIPH_TO_MEMORY,
                       DMA_PDATAALIGN_BYTE, DMA_CIRCULAR)) return false;
    __HAL_LINKDMA(&huart4, hdmarx, force_dma);
    if (!configure_dma(&pc_dma, DMA2_Stream7, DMA_CHANNEL_4, DMA_MEMORY_TO_PERIPH,
                       DMA_PDATAALIGN_BYTE, DMA_NORMAL)) return false;
    __HAL_LINKDMA(&huart1, hdmatx, pc_dma);
    if (!configure_dma(&adc_dma, DMA2_Stream0, DMA_CHANNEL_0, DMA_PERIPH_TO_MEMORY,
                       DMA_PDATAALIGN_HALFWORD, DMA_CIRCULAR)) return false;
    __HAL_LINKDMA(&hadc1, DMA_Handle, adc_dma);

    /* 168MHz SYSCLK 下 APB2=84MHz，ADC=21MHz，三通道 144+12 周期约22.3us。 */
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.ScanConvMode = ENABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
    hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T3_TRGO;
    hadc1.Init.NbrOfConversion = 3;
    hadc1.Init.DMAContinuousRequests = ENABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SEQ_CONV;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) return false;
    ADC_ChannelConfTypeDef c = {0};
    c.SamplingTime = ADC_SAMPLETIME_144CYCLES;
    for (uint32_t a = 0; a < 3; a++) {
        c.Channel = ADC_CHANNEL_10 + a;
        c.Rank = a + 1;
        if (HAL_ADC_ConfigChannel(&hadc1, &c) != HAL_OK) return false;
    }
    uint32_t timer_hz = HAL_RCC_GetPCLK1Freq();
    if ((RCC->CFGR & RCC_CFGR_PPRE1) != 0) timer_hz *= 2;
    if (timer_hz % 1000000U != 0 || 1000000U % TOUCH_SAMPLE_HZ != 0) return false;
    sample_timer.Instance = TIM3;
    sample_timer.Init.Prescaler = timer_hz / 1000000U - 1;
    sample_timer.Init.CounterMode = TIM_COUNTERMODE_UP;
    sample_timer.Init.Period = 1000000U / TOUCH_SAMPLE_HZ - 1;
    sample_timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    sample_timer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&sample_timer) != HAL_OK) return false;
    TIM_MasterConfigTypeDef master = {0};
    master.MasterOutputTrigger = TIM_TRGO_UPDATE;
    master.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&sample_timer, &master) != HAL_OK) return false;
    irq(DMA1_Stream2_IRQn); irq(UART4_IRQn);
    irq(DMA2_Stream0_IRQn); irq(ADC_IRQn);
    irq(DMA2_Stream7_IRQn); irq(USART1_IRQn);
    if (HAL_UARTEx_ReceiveToIdle_DMA(&huart4, rx_dma, sizeof(rx_dma)) != HAL_OK) return false;
    return start_adc();
}
void Touch_UART_RxEvent(UART_HandleTypeDef *h)
{
    if (h != &huart4 || force_broken) return;
    /* UART4 IDLE 与 DMA HT/TC 使用相同抢占优先级；读当前 NDTR 避免重复旧 Size。 */
    uint16_t pos = (uint16_t)(RX_DMA_SIZE - __HAL_DMA_GET_COUNTER(&force_dma));
    if (pos == RX_DMA_SIZE) pos = 0;
    while (rx_pos != pos) {
        if ((uint32_t)(rx_head - rx_tail) == RX_RING_SIZE) {
            stats.ring_overflows++; force_broken = true; break;
        }
        uint8_t byte = rx_dma[rx_pos];
        rx_ring[rx_head % RX_RING_SIZE] = byte;
        if (g_touch_io_debug.first_rx_size < sizeof(g_touch_io_debug.first_rx)) {
            g_touch_io_debug.first_rx[g_touch_io_debug.first_rx_size] = byte;
            g_touch_io_debug.first_rx_size++;
        }
        g_touch_io_debug.force_rx_bytes++;
        __DMB(); rx_head++;
        rx_pos = (uint16_t)((rx_pos + 1U) % RX_DMA_SIZE);
    }
}
bool Touch_BSP_ReadByte(uint8_t *b)
{
    if (force_broken || rx_tail == rx_head) return false;
    __DMB(); *b = rx_ring[rx_tail % RX_RING_SIZE];
    __DMB(); rx_tail++;
    return true;
}
static void adc_ready(unsigned half)
{
    if (adc_broken) return;
    sequence++;
    /* 迟到的中断不能读取 DMA 已在覆写的半区。队列保存完整副本而非裸指针。 */
    uint32_t remaining = __HAL_DMA_GET_COUNTER(&adc_dma);
    unsigned active = (remaining > TOUCH_BLOCK_FRAMES * 3 || remaining == 0) ? 0U : 1U;
    if (active == half || (uint32_t)(block_head - block_tail) == BLOCK_QUEUE_SIZE) {
        stats.adc_overruns++; return;
    }
    touch_adc_block_t *b = &blocks[block_head % BLOCK_QUEUE_SIZE];
    b->timestamp_ms = HAL_GetTick();
    b->sequence = sequence;
    memcpy(b->samples, adc_samples + half * TOUCH_BLOCK_FRAMES * 3, sizeof(b->samples));
    __DMB(); block_head++;
}
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *h) { if (h == &hadc1) adc_ready(0); }
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *h) { if (h == &hadc1) adc_ready(1); }
void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *h)
{
    if (h == &hadc1) { stats.adc_errors++; adc_broken = true; }
}
bool Touch_BSP_ReadBlock(touch_adc_block_t *b)
{
    if (adc_broken || block_head == block_tail) return false;
    __DMB(); *b = blocks[block_tail % BLOCK_QUEUE_SIZE];
    __DMB(); block_tail++;
    for (unsigned i = 0; i < 6; i++) g_touch_adc_debug.dma_first[i] = b->samples[i];
    g_touch_adc_debug.block_sequence = b->sequence;
    return true;
}
void Touch_UART_TxComplete(UART_HandleTypeDef *h)
{
    if (h == &huart1) {
        g_touch_io_debug.pc_tx_completed++;
        pc_busy = false;
    }
}
void Touch_UART_Error(UART_HandleTypeDef *h)
{
    if (h == &huart4) {
        g_touch_io_debug.force_last_error = h->ErrorCode;
        stats.uart_errors++; force_broken = true;
    }
    if (h == &huart1) {
        g_touch_io_debug.pc_last_error = h->ErrorCode;
        stats.pc_errors++; pc_broken = true;
    }
}
bool Touch_BSP_RequestForce(void)
{
    if (force_broken || HAL_UART_Transmit_IT(&huart4, request_all, sizeof(request_all)) != HAL_OK)
        return false;
    g_touch_io_debug.force_queries++;
    return true;
}
bool Touch_BSP_SendPC(const char *data, size_t size)
{
    if (!size) return true;
    if (size > sizeof(pc_tx) || pc_busy || pc_broken) { stats.pc_drops++; return false; }
    memcpy(pc_tx, data, size);
    pc_busy = true;
    if (HAL_UART_Transmit_DMA(&huart1, pc_tx, (uint16_t)size) != HAL_OK) {
        pc_busy = false; pc_broken = true; stats.pc_errors++; stats.pc_drops++; return false;
    }
    g_touch_io_debug.pc_tx_started++;
    return true;
}
bool Touch_BSP_Recover(uint32_t now)
{
    static uint32_t last;
    if (now - last < 100U) return false;
    last = now;
    bool reset_parser = false;
    if (force_broken) {
        reset_parser = true;
        if (HAL_UART_Abort(&huart4) == HAL_OK) {
            rx_head = rx_tail = 0; rx_pos = 0;
            force_broken = false;
            if (HAL_UARTEx_ReceiveToIdle_DMA(&huart4, rx_dma, sizeof(rx_dma)) != HAL_OK)
                force_broken = true;
        }
    }
    if (adc_broken) {
        (void)HAL_TIM_Base_Stop(&sample_timer);
        if (HAL_ADC_Stop_DMA(&hadc1) == HAL_OK) {
            block_head = block_tail = 0;
            adc_broken = false;
            if (!start_adc()) adc_broken = true;
        }
    }
    if (pc_broken && HAL_UART_Abort(&huart1) == HAL_OK) {
        pc_busy = false; pc_broken = false;
    }
    return reset_parser;
}
void Touch_BSP_GetStats(touch_bsp_stats_t *out)
{
    uint32_t mask = __get_PRIMASK(); __disable_irq();
    *out = stats;
    __set_PRIMASK(mask);
    /* 100ms 一次，仅读配置/NDTR；不读 DR 或 SR，避免干扰 UART 清标志序列。 */
    uint32_t now = HAL_GetTick();
    if (now - g_touch_io_debug.snapshot_ms >= 100U) {
        g_touch_io_debug.pc_brr = huart1.Instance->BRR;
        g_touch_io_debug.pc_cr1 = huart1.Instance->CR1;
        g_touch_io_debug.pc_dma_remaining = __HAL_DMA_GET_COUNTER(&pc_dma);
        g_touch_io_debug.force_brr = huart4.Instance->BRR;
        g_touch_io_debug.force_cr1 = huart4.Instance->CR1;
        g_touch_io_debug.force_dma_remaining = __HAL_DMA_GET_COUNTER(&force_dma);
        g_touch_io_debug.pa9_mode = (GPIOA->MODER >> 18) & 3U;
        g_touch_io_debug.pa9_af = (GPIOA->AFR[1] >> 4) & 15U;
        g_touch_io_debug.pc10_mode = (GPIOC->MODER >> 20) & 3U;
        g_touch_io_debug.pc10_af = (GPIOC->AFR[1] >> 8) & 15U;
        g_touch_io_debug.pc11_mode = (GPIOC->MODER >> 22) & 3U;
        g_touch_io_debug.pc11_af = (GPIOC->AFR[1] >> 12) & 15U;
        g_touch_io_debug.snapshot_ms = now;
        /* 不读 ADC DR/SR，只读取配置和 DMA 地址，保持转换清标志流程不变。 */
        uint32_t sqr3 = hadc1.Instance->SQR3;
        for (unsigned a = 0; a < 3; a++) {
            g_touch_adc_debug.pc_mode[a] = (GPIOC->MODER >> (2U * a)) & 3U;
            g_touch_adc_debug.pc_pull[a] = (GPIOC->PUPDR >> (2U * a)) & 3U;
            g_touch_adc_debug.channels[a] = (sqr3 >> (5U * a)) & 31U;
            g_touch_adc_debug.sampling_code[a] = (hadc1.Instance->SMPR1 >> (3U * a)) & 7U;
        }
        g_touch_adc_debug.sequence_length = ((hadc1.Instance->SQR1 >> 20) & 15U) + 1U;
        g_touch_adc_debug.cr1 = hadc1.Instance->CR1;
        g_touch_adc_debug.cr2 = hadc1.Instance->CR2;
        g_touch_adc_debug.common_ccr = ADC->CCR;
        g_touch_adc_debug.resolution_bits = 12U - 2U * ((hadc1.Instance->CR1 >> 24) & 3U);
        g_touch_adc_debug.left_aligned = (hadc1.Instance->CR2 >> 11) & 1U;
        g_touch_adc_debug.adc_clock_hz = HAL_RCC_GetPCLK2Freq() / (2U * (((ADC->CCR >> 16) & 3U) + 1U));
        g_touch_adc_debug.dma_cr = adc_dma.Instance->CR;
        g_touch_adc_debug.dma_remaining = __HAL_DMA_GET_COUNTER(&adc_dma);
        g_touch_adc_debug.dma_peripheral_address = adc_dma.Instance->PAR;
        g_touch_adc_debug.dma_memory_address = adc_dma.Instance->M0AR;
        g_touch_adc_debug.expected_peripheral_address = (uint32_t)&hadc1.Instance->DR;
        g_touch_adc_debug.expected_memory_address = (uint32_t)adc_samples;
        g_touch_adc_debug.snapshot_ms = now;
    }
}
/* 独占入口：CubeMX 不要重复生成这些 IRQ；所有 IRQ 均不解析/打印/阻塞等待。 */
void UART4_IRQHandler(void) { HAL_UART_IRQHandler(&huart4); }
void USART1_IRQHandler(void) { HAL_UART_IRQHandler(&huart1); }
void DMA1_Stream2_IRQHandler(void) { HAL_DMA_IRQHandler(&force_dma); }
void DMA2_Stream0_IRQHandler(void) { HAL_DMA_IRQHandler(&adc_dma); }
void DMA2_Stream7_IRQHandler(void) { HAL_DMA_IRQHandler(&pc_dma); }
void ADC_IRQHandler(void) { HAL_ADC_IRQHandler(&hadc1); }
