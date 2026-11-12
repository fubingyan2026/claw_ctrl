/*
 * Copyright (c) 2026 E1_PRO 项目组
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file    indicator_task.c
 * @brief   双电机状态指示、补光灯命令和控制板心跳任务
 */

#include "indicator_task.h"

#include "drv_can.h"
#include "drv_rgb_led.h"
#include "log.h"
#include "main.h"
#include "srv_indicator.h"
#include "srv_fill_light.h"
#include "srv_juxie.h"
#include "srv_board_protocol.h"
#include "srv_upgrade.h"

#define INDICATOR_TICK_PERIOD_MS (10U)

static bool s_initialized;
static bool s_can_ok;
static bool s_agg_valid;
static juxie_aggregate_t s_last_agg;
static uint32_t s_last_tick_ms;
static uint32_t s_last_rx_drop;
static uint32_t s_last_tx_drop;
static uint32_t s_last_drop_log_ms;

static const char* aggregate_name(juxie_aggregate_t state)
{
    switch (state) {
    case JUXIE_AGG_DISABLED:
        return "disabled";
    case JUXIE_AGG_NORMAL:
        return "normal";
    case JUXIE_AGG_UPGRADE:
        return "upgrade";
    case JUXIE_AGG_FAULT:
        return "fault";
    case JUXIE_AGG_OFF:
    default:
        return "off";
    }
}

void indicator_task_init(void)
{
    drv_rgb_led_init();

    uint32_t now = board_millis();
    srv_juxie_init(now);
    srv_indicator_init(now);

    s_last_tick_ms = now;
    s_initialized = true;

    s_can_ok = drv_can_init();
    if (s_can_ok) {
        srv_board_protocol_init(now);
        if (!srv_upgrade_init()) {
            LOG_E("upgrade", "upgrade metadata unavailable");
        }
    }

    if (!s_can_ok) {
        drv_rgb_led_set(IND_LEVEL_RED, 0U, 0U);
        LOG_E("indicator", "FDCAN init failed");
    } else {
        LOG_I("indicator", "FDCAN ready, Dev_ID=%u/%u",
            (unsigned)JUXIE_DEV_ID_PRIMARY, (unsigned)JUXIE_DEV_ID_MIRROR);
    }
}

void indicator_task_step(void)
{
    if (!s_initialized || !s_can_ok) {
        return;
    }

    uint32_t now = board_millis();

    /* 先把 TX 队列提交到硬件，再处理 RX 队列 */
    drv_can_poll();

    drv_can_msg_t msg;
    while (drv_can_recv(&msg)) {
        if (msg.id == BOARD_CAN_LIGHT_COMMAND_ID) {
            (void)srv_board_protocol_on_frame(&msg, now);
        } else if (msg.id == BOARD_CAN_UPGRADE_HOST_ID) {
            srv_upgrade_on_frame(&msg, board_millis());
        } else {
            srv_juxie_on_frame(&msg, now);
        }
    }

    if (srv_juxie_should_poll(now)) {
        (void)drv_can_send_sync();
        srv_juxie_mark_poll_sent(now);
    }

    uint32_t rx_drop = drv_can_get_rx_dropped();
    uint32_t tx_drop = drv_can_get_tx_dropped();
    if ((rx_drop != s_last_rx_drop || tx_drop != s_last_tx_drop)
        && (now - s_last_drop_log_ms) >= 1000U) {
        LOG_W("can", "queue drop rx=%lu tx=%lu",
            (unsigned long)rx_drop, (unsigned long)tx_drop);
        s_last_rx_drop = rx_drop;
        s_last_tx_drop = tx_drop;
        s_last_drop_log_ms = now;
    }

    srv_board_protocol_step(now);

    /* 把本轮新入队的同步帧和状态帧尽快提交 */
    drv_can_poll();
    now = board_millis();
    const bool reboot_ready = srv_upgrade_step(now);
    if (now - s_last_tick_ms < INDICATOR_TICK_PERIOD_MS) {
        return;
    }

    s_last_tick_ms = now;
    srv_juxie_tick(now);
    const juxie_aggregate_t aggregate = srv_upgrade_active() ? JUXIE_AGG_UPGRADE : srv_juxie_get_aggregate();
    /* ACK can leave immediately. Install once the displayed blue cycle finishes. */
    const bool finish_upgrade = reboot_ready && s_agg_valid && s_last_agg == JUXIE_AGG_UPGRADE;
    const juxie_aggregate_t displayed = srv_indicator_update(finish_upgrade ? JUXIE_AGG_OFF : aggregate, now);
    if (finish_upgrade && displayed == JUXIE_AGG_OFF) {
        board_install_upgrade(srv_fill_light_get_target());
    }
    if (!s_agg_valid || displayed != s_last_agg) {
        s_last_agg = displayed;
        s_agg_valid = true;
        LOG_I("indicator", "state -> %s", aggregate_name(displayed));
    }
}
