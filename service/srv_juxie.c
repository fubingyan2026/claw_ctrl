/*
 * Copyright (c) 2026 E1_PRO 项目组
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file    srv_juxie.c
 * @brief   巨蟹电机反馈解析、双电机状态机、升级会话检测
 */

#include "srv_juxie.h"

#include <string.h>

#define JUXIE_BOOT_CMD_START (0x01U)
#define JUXIE_BOOT_CMD_METADATA (0x02U)
#define JUXIE_BOOT_CMD_DATA (0x03U)
#define JUXIE_BOOT_CMD_VERIFY (0x04U)
#define JUXIE_BOOT_CMD_REBOOT (0x05U)
#define JUXIE_BOOT_CMD_CANCEL (0x06U)
#define JUXIE_BOOT_CMD_DATA_START (0x07U)
#define JUXIE_BOOT_CMD_DATA_END (0x08U)

typedef struct {
    bool ever_seen;
    bool enabled;
    bool explicit_fault;
    bool link_lost;
    uint16_t error_code;
    uint32_t last_feedback_ms;
    juxie_motor_state_t state;
} juxie_motor_ctx_t;

static const uint8_t s_dev_ids[JUXIE_MOTOR_COUNT] = {
    JUXIE_DEV_ID_PRIMARY,
    JUXIE_DEV_ID_MIRROR,
};

static juxie_motor_ctx_t s_motors[JUXIE_MOTOR_COUNT];
static bool s_initialized;
static bool s_upgrade_active;
static uint32_t s_last_boot_ms;
static uint32_t s_last_control_ms;
static uint32_t s_last_poll_ms;

static int motor_index_from_dev_id(uint32_t dev_id)
{
    for (uint8_t i = 0; i < JUXIE_MOTOR_COUNT; i++) {
        if (s_dev_ids[i] == dev_id) {
            return (int)i;
        }
    }
    return -1;
}

static bool is_control_id(uint32_t id)
{
    if (id == 0x80U) {
        return true;
    }
    if ((id & 0x780U) == 0x100U) {
        return true;
    }
    if (id == 0x200U || id == 0x210U) {
        return true;
    }
    if ((id & 0x780U) == 0x600U) {
        return true;
    }
    return false;
}

static void handle_feedback(const drv_can_msg_t* msg, uint32_t now_ms)
{
    uint8_t len = msg->dlc;
    if (len != 12U && len != 16U) {
        return;
    }

    int index = motor_index_from_dev_id(msg->id & 0x7FU);
    if (index < 0) {
        return;
    }

    uint8_t status = msg->data[len - 1U];
    uint16_t error_code = ((uint16_t)msg->data[6] << 8) | msg->data[7];

    juxie_motor_ctx_t* motor = &s_motors[index];
    motor->ever_seen = true;
    motor->last_feedback_ms = now_ms;
    motor->enabled = (status & JUXIE_STATUS_ENABLED) != 0U;
    motor->error_code = error_code;
    motor->explicit_fault = ((status & JUXIE_STATUS_ERROR) != 0U) || (error_code != 0U);
    motor->link_lost = false;
}

static void handle_boot_frame(const drv_can_msg_t* msg, uint32_t now_ms)
{
    if (msg->id != JUXIE_BOOT_HOST_ID || msg->dlc < 1U) {
        return;
    }

    uint8_t cmd = msg->data[0];
    switch (cmd) {
    case JUXIE_BOOT_CMD_START:
        s_upgrade_active = true;
        s_last_boot_ms = now_ms;
        break;
    case JUXIE_BOOT_CMD_METADATA:
    case JUXIE_BOOT_CMD_DATA:
    case JUXIE_BOOT_CMD_VERIFY:
    case JUXIE_BOOT_CMD_DATA_START:
    case JUXIE_BOOT_CMD_DATA_END:
        if (s_upgrade_active) {
            s_last_boot_ms = now_ms;
        }
        break;
    case JUXIE_BOOT_CMD_REBOOT:
    case JUXIE_BOOT_CMD_CANCEL:
        s_upgrade_active = false;
        break;
    default:
        break;
    }
}

void srv_juxie_init(uint32_t now_ms)
{
    memset(s_motors, 0, sizeof(s_motors));
    for (uint8_t i = 0; i < JUXIE_MOTOR_COUNT; i++) {
        s_motors[i].state = JUXIE_MOTOR_UNKNOWN;
        s_motors[i].last_feedback_ms = now_ms;
    }

    s_upgrade_active = false;
    s_last_boot_ms = now_ms;
    s_last_control_ms = now_ms;
    s_last_poll_ms = now_ms;
    s_initialized = true;
}

void srv_juxie_on_frame(const drv_can_msg_t* msg, uint32_t now_ms)
{
    if (!s_initialized || msg == 0 || msg->extended) {
        return;
    }

    if (is_control_id(msg->id)) {
        s_last_control_ms = now_ms;
    }

    if (msg->id == JUXIE_BOOT_HOST_ID) {
        handle_boot_frame(msg, now_ms);
        return;
    }

    if ((msg->id & 0x780U) == JUXIE_FEEDBACK_BASE) {
        handle_feedback(msg, now_ms);
    }
}

void srv_juxie_tick(uint32_t now_ms)
{
    if (!s_initialized) {
        return;
    }

    if (s_upgrade_active && (now_ms - s_last_boot_ms) > JUXIE_BOOT_TIMEOUT_MS) {
        s_upgrade_active = false;
    }

    for (uint8_t i = 0; i < JUXIE_MOTOR_COUNT; i++) {
        juxie_motor_ctx_t* motor = &s_motors[i];

        motor->link_lost = !s_upgrade_active &&
            (now_ms - motor->last_feedback_ms) > JUXIE_LINK_TIMEOUT_MS;

        if (motor->explicit_fault || motor->link_lost) {
            motor->state = JUXIE_MOTOR_FAULT;
        } else if (!motor->ever_seen) {
            motor->state = JUXIE_MOTOR_UNKNOWN;
        } else if (motor->enabled) {
            motor->state = JUXIE_MOTOR_NORMAL;
        } else {
            motor->state = JUXIE_MOTOR_DISABLED;
        }
    }
}

juxie_motor_state_t srv_juxie_get_motor(uint8_t index)
{
    if (index >= JUXIE_MOTOR_COUNT) {
        return JUXIE_MOTOR_UNKNOWN;
    }
    return s_motors[index].state;
}

juxie_aggregate_t srv_juxie_get_aggregate(void)
{
    bool any_fault = false;
    bool all_normal = true;
    uint8_t detected_count = 0U;

    for (uint8_t i = 0; i < JUXIE_MOTOR_COUNT; i++) {
        if (s_motors[i].ever_seen) {
            ++detected_count;
        }
        if (s_motors[i].state == JUXIE_MOTOR_FAULT) {
            any_fault = true;
        }
        if (s_motors[i].state != JUXIE_MOTOR_NORMAL) {
            all_normal = false;
        }
    }

    if (any_fault || (detected_count > 0U && detected_count < JUXIE_MOTOR_COUNT)) {
        return JUXIE_AGG_FAULT;
    }
    if (s_upgrade_active) {
        return JUXIE_AGG_UPGRADE;
    }
    if (detected_count == 0U) {
        return JUXIE_AGG_WAITING;
    }
    if (!all_normal) {
        return JUXIE_AGG_DISABLED;
    }
    return JUXIE_AGG_NORMAL;
}

bool srv_juxie_get_upgrade_active(void)
{
    return s_upgrade_active;
}

bool srv_juxie_should_poll(uint32_t now_ms)
{
    if (!s_initialized || s_upgrade_active) {
        return false;
    }
    if ((now_ms - s_last_control_ms) < JUXIE_CONTROL_IDLE_MS) {
        return false;
    }
    if ((now_ms - s_last_poll_ms) < JUXIE_SYNC_PERIOD_MS) {
        return false;
    }
    return true;
}

void srv_juxie_mark_poll_sent(uint32_t now_ms)
{
    s_last_poll_ms = now_ms;
}
