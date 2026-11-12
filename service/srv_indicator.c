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
static uint32_t s_phase_start_ms;
static bool s_lit;

static bool is_blinking(juxie_aggregate_t state)
{
    return state == JUXIE_AGG_DISABLED || state == JUXIE_AGG_UPGRADE;
}

void srv_indicator_init(uint32_t now_ms)
{
    s_state = JUXIE_AGG_OFF;
    s_phase_start_ms = now_ms;
    s_lit = false;
    s_initialized = true;
}

juxie_aggregate_t srv_indicator_update(juxie_aggregate_t state, uint32_t now_ms)
{
    if (!s_initialized) {
        return JUXIE_AGG_OFF;
    }

    const bool phase_done = now_ms - s_phase_start_ms >= IND_BLINK_HALF_MS;
    /* Start each half-period when the output changes, even if a tick runs late. */
    if (!is_blinking(s_state) || (!s_lit && phase_done)) {
        s_state = state;
        s_lit = true;
        s_phase_start_ms = now_ms;
    } else if (s_lit && phase_done) {
        s_lit = false;
        s_phase_start_ms = now_ms;
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
        if (s_lit) {
            g = IND_LEVEL_GREEN;
        }
        break;
    case JUXIE_AGG_UPGRADE:
        if (s_lit) {
            b = IND_LEVEL_BLUE;
        }
        break;
    case JUXIE_AGG_WAITING:
    case JUXIE_AGG_OFF:
    default:
        break;
    }

    drv_rgb_led_set(r, g, b);
    return s_state;
}
