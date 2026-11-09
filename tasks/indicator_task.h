/*
 * Copyright (c) 2026 E1_PRO 项目组
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file    indicator_task.h
 * @brief   双电机状态指示灯任务
 */

#ifndef __INDICATOR_TASK_H
#define __INDICATOR_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

void indicator_task_init(void);
void indicator_task_step(void);

#ifdef __cplusplus
}
#endif

#endif /* __INDICATOR_TASK_H */
