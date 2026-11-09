/* USART0 PB6=TX / PB7=RX (AF0), 115200 8N1, DMA0 CH3 TX. */
#include "drv_uart.h"
#include "main.h"
static bool s_initialized, s_active;
drv_uart_error_t drv_uart_init(void)
{
    rcu_periph_clock_enable(RCU_USART0);
    rcu_periph_clock_enable(RCU_DMA0);
    rcu_periph_clock_enable(RCU_DMAMUX);
    gpio_af_set(GPIOB, GPIO_AF_0, GPIO_PIN_6 | GPIO_PIN_7);
    gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_6 | GPIO_PIN_7);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_LEVEL1, GPIO_PIN_6);
    usart_deinit(USART0);
    usart_baudrate_set(USART0, 115200U);
    usart_word_length_set(USART0, USART_WL_8BIT);
    usart_stop_bit_set(USART0, USART_STB_1BIT);
    usart_parity_config(USART0, USART_PM_NONE);
    usart_receive_config(USART0, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART0, USART_TRANSMIT_ENABLE);
    usart_enable(USART0);
    dma_deinit(DMA0, DMA_CH3);
    s_active = false;
    s_initialized = true;
    return DRV_UART_OK;
}
bool drv_uart_is_initialized(drv_uart_channel_t ch)
{
    return ch == DRV_UART_CH_1 && s_initialized;
}
bool drv_uart_is_tx_busy(drv_uart_channel_t ch)
{
    if (ch != DRV_UART_CH_1 || !s_initialized || !s_active)
        return false;
    if (dma_flag_get(DMA0, DMA_CH3, DMA_INTF_FTFIF) == RESET || usart_flag_get(USART0, USART_FLAG_TC) == RESET)
        return true;
    dma_channel_disable(DMA0, DMA_CH3);
    usart_dma_transmit_config(USART0, USART_DENT_DISABLE);
    s_active = false;
    return false;
}
drv_uart_error_t drv_uart_send(drv_uart_channel_t ch, const uint8_t* data, uint32_t len)
{
    if (!s_initialized)
        return DRV_UART_ERROR_UNINITIALIZED;
    if (ch != DRV_UART_CH_1 || !data || !len || len > 65535U)
        return DRV_UART_ERROR_INVALID_PARAM;
    if (drv_uart_is_tx_busy(ch))
        return DRV_UART_ERROR_TX_BUSY;
    dma_parameter_struct cfg;
    dma_deinit(DMA0, DMA_CH3);
    dma_struct_para_init(&cfg);
    cfg.request = DMA_REQUEST_USART0_TX;
    cfg.direction = DMA_MEMORY_TO_PERIPHERAL;
    cfg.memory_addr = (uint32_t)data;
    cfg.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    cfg.memory_width = DMA_MEMORY_WIDTH_8BIT;
    cfg.number = len;
    cfg.periph_addr = (uint32_t)&USART_DATA(USART0);
    cfg.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    cfg.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;
    cfg.priority = DMA_PRIORITY_MEDIUM;
    dma_init(DMA0, DMA_CH3, &cfg);
    dma_circulation_disable(DMA0, DMA_CH3);
    dma_memory_to_memory_disable(DMA0, DMA_CH3);
    dmamux_synchronization_disable(DMAMUX_MUXCH3);
    usart_flag_clear(USART0, USART_FLAG_TC);
    s_active = true;
    dma_channel_enable(DMA0, DMA_CH3);
    usart_dma_transmit_config(USART0, USART_DENT_ENABLE);
    return DRV_UART_OK;
}
