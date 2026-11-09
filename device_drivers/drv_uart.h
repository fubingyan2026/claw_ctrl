/*
 * Copyright (c) 2026 E1_PRO 项目组
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/** @brief USART0 TX DMA, PB6(TX)/PB7(RX), 115200 8N1. RX DMA is not enabled. */

#ifndef __DRV_UART_H
#define __DRV_UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    DRV_UART_CH_1 = 0, /**< USART0 — PB6(TX) / PB7(RX), 115200 baud */
    DRV_UART_CH_NUM,
} drv_uart_channel_t;

typedef enum {
    DRV_UART_OK = 0,
    DRV_UART_ERROR_NULL_PTR,
    DRV_UART_ERROR_UNINITIALIZED,
    DRV_UART_ERROR_TX_BUSY,
    DRV_UART_ERROR_INVALID_PARAM,
    DRV_UART_ERROR_HAL,
} drv_uart_error_t;

drv_uart_error_t drv_uart_init(void);
bool drv_uart_is_initialized(drv_uart_channel_t ch);

/** @brief 非阻塞 DMA 发送 */
drv_uart_error_t drv_uart_send(drv_uart_channel_t ch, const uint8_t* data, uint32_t len);

/** @brief 查询 TX DMA 是否忙碌 */
bool drv_uart_is_tx_busy(drv_uart_channel_t ch);

#ifdef __cplusplus
}
#endif

#endif /* __DRV_UART_H */
