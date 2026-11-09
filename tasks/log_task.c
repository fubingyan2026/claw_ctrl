/*
 * Copyright (c) 2026 E1_PRO 项目组
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file    log_task.c
 * @brief   日志输出任务实现（主循环周期调度，USART0 DMA）
 */

#include "log_task.h"

#include "drv_uart.h"
#include "log.h"
#include "main.h"

#define LOG_TASK_TX_BUF_SIZE (256U)
#define LOG_TASK_PERIOD_MS (1U)

static uint8_t s_tx_buf[LOG_TASK_TX_BUF_SIZE];
static log_task_output_t s_output_mode = LOG_OUTPUT_UART;
static uint32_t s_last_step_ms;
static bool s_initialized;

void log_task_init(void)
{
    log_config_t cfg = {
        .name = "E1_PRO",
        .tx_buffer = NULL,
        .get_timestamp_cb = board_millis,
        .tx_buffer_size = 0,
        .format_buffer_size = LOG_DEFAULT_FORMAT_BUFFER_SIZE,
        .default_level = LOG_LEVEL_DEBUG,
        .enable_color = true,
        .enable_timestamp = true,
    };

    (void)log_init(&cfg);
    (void)log_set_level(LOG_LEVEL_DEBUG);
    (void)drv_uart_init();

    s_last_step_ms = board_millis();
    s_initialized = true;

    LOG_I("log", "log task ready");
}

void log_task_set_output(log_task_output_t mode)
{
    s_output_mode = mode;
}

void log_task_step(void)
{
    if (!s_initialized) {
        return;
    }

    uint32_t now = board_millis();
    if ((now - s_last_step_ms) < LOG_TASK_PERIOD_MS) {
        return;
    }
    s_last_step_ms = now;

    if (s_output_mode != LOG_OUTPUT_UART || drv_uart_is_tx_busy(DRV_UART_CH_1)) {
        return;
    }

    uint32_t len = log_tx_len();
    if (len == 0U) {
        return;
    }
    if (len > sizeof(s_tx_buf)) {
        len = sizeof(s_tx_buf);
    }

    len = log_tx_get(s_tx_buf, len);
    if (len > 0U) {
        (void)drv_uart_send(DRV_UART_CH_1, s_tx_buf, len);
    }
}
