#include "srv_board_protocol.h"
#include "drv_fill_light.h"
#include "srv_fill_light.h"
#include "srv_juxie.h"

static bool s_initialized;
static uint32_t s_last_heartbeat_ms;

/* Wire values are explicit, independent of the internal enum numbering. */
static uint8_t motor_state_to_wire(juxie_aggregate_t state)
{
    switch (state) {
    case JUXIE_AGG_OFF: return 0x00U;
    case JUXIE_AGG_WAITING:
    case JUXIE_AGG_DISABLED: return 0x01U;
    case JUXIE_AGG_NORMAL: return 0x02U;
    case JUXIE_AGG_UPGRADE: return 0x03U;
    case JUXIE_AGG_FAULT: return 0x04U;
    default: return 0x04U;
    }
}

static void send_status(uint32_t now_ms)
{
    /* Account for recent feedback and timeouts before taking the snapshot. */
    srv_juxie_tick(now_ms);
    uint16_t level = srv_fill_light_get_target();
    drv_can_msg_t status = {
        .id = BOARD_CAN_STATUS_ID,
        .fd = true,
        .dlc = BOARD_PROTOCOL_LENGTH,
    };
    status.data[0] = BOARD_PROTOCOL_VERSION;
    status.data[1] = motor_state_to_wire(srv_juxie_get_aggregate());
    status.data[2] = (uint8_t)level;
    status.data[3] = (uint8_t)(level >> 8);
    for (unsigned i = 0; i < 4U; ++i) {
        status.data[4U + i] = (uint8_t)(now_ms >> (8U * i));
    }
    /* Best effort: never block the control loop or retry a full TX queue here. */
    (void)drv_can_send(&status);
}

void srv_board_protocol_init(uint32_t now_ms)
{
    s_last_heartbeat_ms = now_ms;
    s_initialized = true;
}

bool srv_board_protocol_on_frame(const drv_can_msg_t *msg, uint32_t now_ms)
{
    if (!s_initialized || !msg || msg->extended || !msg->fd ||
        msg->id != BOARD_CAN_LIGHT_COMMAND_ID || msg->dlc != BOARD_PROTOCOL_LENGTH ||
        msg->data[0] != BOARD_PROTOCOL_VERSION || msg->data[1] != BOARD_LIGHT_SET_COMMAND) {
        return false;
    }
    for (unsigned i = 4U; i < BOARD_PROTOCOL_LENGTH; ++i) {
        if (msg->data[i] != 0U) { return false; }
    }
    uint16_t level = (uint16_t)msg->data[2] | ((uint16_t)msg->data[3] << 8);
    if (level > DRV_FILL_LIGHT_LEVEL_MAX) { return false; }

    srv_fill_light_set_target(level, now_ms);
    send_status(now_ms);
    return true;
}

void srv_board_protocol_step(uint32_t now_ms)
{
    if (!s_initialized || (uint32_t)(now_ms - s_last_heartbeat_ms) < BOARD_HEARTBEAT_PERIOD_MS) {
        return;
    }
    /* Skip missed periods rather than flooding the bus after a long delay. */
    s_last_heartbeat_ms = now_ms;
    send_status(now_ms);
}
