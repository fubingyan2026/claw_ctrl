/*
 * Copyright (c) 2026 E1_PRO 项目组
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file    srv_indicator.c
 * @brief   双电机汇总状态到 RGB 灯效的映射
 */

#include "srv_indicator.h"

#include "drv_rgb_led.h"

static bool s_initialized;
static juxie_aggregate_t s_state;
static uint32_t s_state_start_ms;

static bool blink_on(uint32_t now_ms)
{
    return (((now_ms - s_state_start_ms) / IND_BLINK_HALF_MS) & 1U) == 0U;
}

void srv_indicator_init(uint32_t now_ms)
{
    s_state = JUXIE_AGG_OFF;
    s_state_start_ms = now_ms;
    s_initialized = true;
}

void srv_indicator_update(juxie_aggregate_t state, uint32_t now_ms)
{
    if (!s_initialized) {
        return;
    }

    if (state != s_state) {
        s_state = state;
        s_state_start_ms = now_ms;
    }

    uint16_t r = 0;
    uint16_t g = 0;
    uint16_t b = 0;

    switch (s_state) {
    case JUXIE_AGG_NORMAL:
        r = IND_LEVEL_WHITE_R;
        g = IND_LEVEL_WHITE_G;
        b = IND_LEVEL_WHITE_B;
        break;
    case JUXIE_AGG_FAULT:
        r = IND_LEVEL_RED;
        break;
    case JUXIE_AGG_DISABLED:
        if (blink_on(now_ms)) {
            g = IND_LEVEL_GREEN;
        }
        break;
    case JUXIE_AGG_UPGRADE:
        if (blink_on(now_ms)) {
            b = IND_LEVEL_BLUE;
        }
        break;
    case JUXIE_AGG_OFF:
    default:
        break;
    }

    drv_rgb_led_set(r, g, b);
}
