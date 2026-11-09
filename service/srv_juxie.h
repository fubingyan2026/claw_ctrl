/*
 * Copyright (c) 2026 E1_PRO 项目组
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file    srv_juxie.h
 * @brief   巨蟹电机反馈解析、双电机状态机、升级会话检测
 */

#ifndef __SRV_JUXIE_H
#define __SRV_JUXIE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "drv_can.h"

#define JUXIE_MOTOR_COUNT (2U)
#define JUXIE_FEEDBACK_BASE (0x300U)

#ifndef JUXIE_DEV_ID_PRIMARY
#define JUXIE_DEV_ID_PRIMARY (1U)
#endif
#ifndef JUXIE_DEV_ID_MIRROR
#define JUXIE_DEV_ID_MIRROR (2U)
#endif

#ifndef JUXIE_LINK_TIMEOUT_MS
#define JUXIE_LINK_TIMEOUT_MS (1500U)
#endif
#ifndef JUXIE_CONTROL_IDLE_MS
#define JUXIE_CONTROL_IDLE_MS (500U)
#endif
#ifndef JUXIE_SYNC_PERIOD_MS
#define JUXIE_SYNC_PERIOD_MS (500U)
#endif
#ifndef JUXIE_BOOT_HOST_ID
#define JUXIE_BOOT_HOST_ID (0x701U)
#endif
/* 修改 JUXIE_BOOT_HOST_ID 时，需同步修改 drv_can.c 中的 0x701 硬件滤波器 */
#ifndef JUXIE_BOOT_TIMEOUT_MS
#define JUXIE_BOOT_TIMEOUT_MS (6000U)
#endif

#define JUXIE_STATUS_ENABLED (0x80U)
#define JUXIE_STATUS_ERROR (0x20U)

typedef enum {
    JUXIE_MOTOR_UNKNOWN = 0,
    JUXIE_MOTOR_DISABLED,
    JUXIE_MOTOR_NORMAL,
    JUXIE_MOTOR_FAULT,
} juxie_motor_state_t;

typedef enum {
    JUXIE_AGG_OFF = 0,
    JUXIE_AGG_DISABLED,
    JUXIE_AGG_NORMAL,
    JUXIE_AGG_UPGRADE,
    JUXIE_AGG_FAULT,
} juxie_aggregate_t;

void srv_juxie_init(uint32_t now_ms);

/** @brief 主循环上下文处理一帧（从 CAN RX msg_fifo 取出） */
void srv_juxie_on_frame(const drv_can_msg_t* msg, uint32_t now_ms);

/** @brief 周期任务：更新超时与单台状态 */
void srv_juxie_tick(uint32_t now_ms);

juxie_motor_state_t srv_juxie_get_motor(uint8_t index);
juxie_aggregate_t srv_juxie_get_aggregate(void);
bool srv_juxie_get_upgrade_active(void);

bool srv_juxie_should_poll(uint32_t now_ms);
void srv_juxie_mark_poll_sent(uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* __SRV_JUXIE_H */
