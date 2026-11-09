/*
 * Copyright (c) 2026 E1_PRO 项目组
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file    log_task.h
 * @brief   日志输出任务（主循环 20 ms 调度，USART1 DMA）
 */

#ifndef LOG_TASK_H
#define LOG_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LOG_OUTPUT_NONE = 0,
    LOG_OUTPUT_UART,
} log_task_output_t;

void log_task_init(void);
void log_task_step(void);
void log_task_set_output(log_task_output_t mode);

#ifdef __cplusplus
}
#endif

#endif /* LOG_TASK_H */
