#include "srv_upgrade.h"
#include "board_protocol.h"
#include "drv_flash.h"
#include "rs_crc32.h"
#include "upgrade_storage.h"
#include <string.h>

#define BLOCK_SIZE 1024U
#define BLOCK_TIMEOUT_MS 100U
#define SESSION_TIMEOUT_MS 6000U
#define REBOOT_TIMEOUT_MS 100U
#define HW_ID 0x0001U
#define INFO_PAGE_BYTES 5U
#define INFO_PAGE_COUNT 5U

enum {
    START = 0x01, METADATA, DATA, VERIFY, REBOOT, CANCEL, DATA_START, DATA_END,
    GET_INFO = 0x09,
    ACK = 0x10, NACK, INFO = 0x12,
};
enum {
    OK = 0x00, BLOCK_CHECKSUM, FLASH_WRITE_ERR, FLASH_VERIFY_ERR, IMAGE_CHECKSUM,
    INVALID_FRAME, INVALID_STATE, TIMEOUT, HW_MISMATCH, FLASH_ERASE_ERR,
    FRAME_SIZE = 0x0B, FW_TOO_BIG, BLOCK_INDEX_MISMATCH,
};
typedef enum { IDLE, WAIT_METADATA, RECEIVING, VERIFIED, RESET_PENDING } upgrade_state_t;

static struct {
    upgrade_state_t state;
    upgrade_image_t image;
    uint32_t checksum;
    uint32_t last_frame_ms;
    uint16_t next_block;
    uint16_t used;
    uint8_t frame_size;
    uint8_t seq;
    bool metadata_valid;
    bool block_active;
    bool reboot_ack_queued;
    uint8_t block[BLOCK_SIZE];
} s;

_Static_assert(FLASH_PAGE_SIZE % BLOCK_SIZE == 0U, "Blocks must fit within a page");

__attribute__((used, section(".app_header")))
static const uint32_t app_header[] = { APP_MAGIC, APP_TARGET, APP_VERSION_MAGIC, APP_FIRMWARE_VERSION };

static uint16_t be16(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] << 8 | data[1]);
}

static uint32_t be32(const uint8_t *data)
{
    return ((uint32_t)be16(data) << 16) | be16(data + 2);
}

static void put_be32(uint8_t *data, uint32_t value)
{
    for (unsigned i = 0; i < 4U; ++i) {
        data[i] = (uint8_t)(value >> (24U - 8U * i));
    }
}

static void send_info(uint8_t token)
{
    const upgrade_metadata_t *meta = upgrade_metadata_get();
    uint8_t payload[INFO_PAGE_BYTES * INFO_PAGE_COUNT] = {
        [20] = (uint8_t)(HW_ID >> 8),
        [21] = (uint8_t)HW_ID,
    };
    put_be32(payload, APP_FIRMWARE_VERSION);
    if (s.metadata_valid) {
        put_be32(payload + 4, meta->stats.reboot_count);
        put_be32(payload + 8, upgrade_metadata_revision());
        put_be32(payload + 12, meta->active.size);
        put_be32(payload + 16, meta->active.crc);
        payload[22] = 1U | (meta->pending.size != 0U ? 2U : 0U);
        payload[23] = (uint8_t)meta->stats.last_result;
        payload[24] = (uint8_t)meta->stats.last_error;
    }
    drv_can_msg_t response = {
        .id = BOARD_CAN_UPGRADE_NODE_ID,
        .dlc = 8,
        .data = { INFO, token },
    };
    for (uint8_t page = 0; page < INFO_PAGE_COUNT; ++page) {
        response.data[2] = page;
        memcpy(response.data + 3, payload + page * INFO_PAGE_BYTES, INFO_PAGE_BYTES);
        if (!drv_can_send(&response)) {
            break;
        }
    }
}

static uint32_t checksum(const uint8_t *data, uint32_t len)
{
    uint32_t sum = 0;
    for (uint32_t i = 0; i < len; ++i) {
        sum += data[i];
    }
    return sum;
}

static bool reply(uint8_t cmd, uint8_t error, uint16_t index)
{
    const drv_can_msg_t response = {
        .id = BOARD_CAN_UPGRADE_NODE_ID,
        .dlc = 8,
        .data = { error == OK ? ACK : NACK, cmd, error, (uint8_t)(index >> 8), (uint8_t)index },
    };
    return drv_can_send(&response);
}

static uint32_t block_offset(void)
{
    return (uint32_t)s.next_block * BLOCK_SIZE;
}

static uint8_t start(const drv_can_msg_t *msg)
{
    /* G474 and canable use cmd, size[4], hw[2], frame size. */
    if (msg->dlc != 8U) {
        return INVALID_FRAME;
    }
    if (!s.metadata_valid || upgrade_metadata_get()->pending.size != 0U) {
        return INVALID_STATE;
    }
    if (be16(msg->data + 5) != HW_ID) {
        return HW_MISMATCH;
    }
    const uint32_t size = be32(msg->data + 1);
    if (size < APP_MIN_SIZE) {
        return INVALID_FRAME;
    }
    if (size > APP_SLOT_SIZE) {
        return FW_TOO_BIG;
    }
    const uint8_t frame_size = msg->data[7];
    switch (frame_size) {
    case 8: case 12: case 16: case 20: case 24: case 32: case 48: case 64:
        break;
    default:
        return FRAME_SIZE;
    }
    s.image = (upgrade_image_t){ .size = size };
    s.frame_size = frame_size;
    s.next_block = 0;
    s.block_active = false;
    s.state = WAIT_METADATA;
    return OK;
}

static uint8_t start_block(const drv_can_msg_t *msg)
{
    if (msg->dlc < 4U || msg->data[1] != 0U) {
        return INVALID_FRAME;
    }
    if (be16(msg->data + 2) != s.next_block || block_offset() >= s.image.size) {
        return BLOCK_INDEX_MISMATCH;
    }
    const uint32_t offset = block_offset();
    /* The host waits for this ACK. Erase one page before its first block. */
    if (offset % FLASH_PAGE_SIZE == 0U &&
        drv_flash_erase(APP_B_BASE + offset, FLASH_PAGE_SIZE) != 0) {
        s.state = IDLE;
        return FLASH_ERASE_ERR;
    }
    s.used = 0;
    s.seq = 0;
    s.block_active = true;
    return OK;
}

static uint8_t receive_block(const drv_can_msg_t *msg, bool end)
{
    const uint32_t left = BLOCK_SIZE - s.used;
    const uint8_t payload = s.frame_size - 2U;
    const uint32_t required = end ? 4U + left : s.frame_size;
    if (!s.block_active || msg->dlc < required || msg->dlc > s.frame_size ||
        msg->data[1] != s.seq || (!end && left <= payload)) {
        s.block_active = false;
        return INVALID_FRAME;
    }
    if (!end) {
        memcpy(s.block + s.used, msg->data + 2, payload);
        s.used += payload;
        ++s.seq;
        return OK;
    }

    s.block_active = false;
    /* The last wire frame can include DLC padding beyond the 1 KiB block. */
    memcpy(s.block + s.used, msg->data + 4, left);
    if ((uint16_t)checksum(s.block, BLOCK_SIZE) != be16(msg->data + 2)) {
        return BLOCK_CHECKSUM;
    }
    const uint32_t addr = APP_B_BASE + block_offset();
    if (drv_flash_write(addr, s.block, BLOCK_SIZE) != 0) {
        s.state = IDLE;
        return FLASH_WRITE_ERR;
    }
    if (memcmp((const void *)addr, s.block, BLOCK_SIZE) != 0) {
        s.state = IDLE;
        return FLASH_VERIFY_ERR;
    }
    ++s.next_block;
    return OK;
}

static uint8_t verify(void)
{
    if (s.state == VERIFIED) {
        return OK;
    }
    if (s.state != RECEIVING || block_offset() < s.image.size) {
        return INVALID_STATE;
    }
    const uint8_t *data = (const uint8_t *)APP_B_BASE;
    if (checksum(data, s.image.size) != s.checksum) {
        s.state = IDLE;
        return IMAGE_CHECKSUM;
    }
    if (!upgrade_image_header_valid(data, s.image.size)) {
        s.state = IDLE;
        return HW_MISMATCH;
    }
    s.image.crc = rs_crc32(data, s.image.size);
    (void)upgrade_image_read_version(data, s.image.size, &s.image.version);
    s.state = VERIFIED;
    return OK;
}

static uint8_t handle_command(const drv_can_msg_t *msg)
{
    switch (msg->data[0]) {
    case START:
        return start(msg);
    case METADATA:
        /* cmd, checksum[4], version[2], optional DLC padding. */
        if (msg->dlc < 7U) {
            return INVALID_FRAME;
        }
        if (s.state != WAIT_METADATA) {
            return INVALID_STATE;
        }
        s.checksum = be32(msg->data + 1);
        s.image.version = be16(msg->data + 5);
        s.state = RECEIVING;
        return OK;
    case DATA_START:
        return s.state == RECEIVING ? start_block(msg) : INVALID_STATE;
    case DATA:
    case DATA_END:
        return s.state == RECEIVING ? receive_block(msg, msg->data[0] == DATA_END) : INVALID_STATE;
    case VERIFY:
        return verify();
    case REBOOT: {
        if (s.state != VERIFIED) {
            return INVALID_STATE;
        }
        upgrade_metadata_t next = *upgrade_metadata_get();
        next.pending = s.image;
        if (!upgrade_metadata_save(&next)) {
            return FLASH_WRITE_ERR;
        }
        s.state = RESET_PENDING;
        return OK;
    }
    case CANCEL:
        s.state = IDLE;
        return OK;
    default:
        return INVALID_FRAME;
    }
}

bool srv_upgrade_init(void)
{
    memset(&s, 0, sizeof(s));
    s.metadata_valid = upgrade_metadata_init();
    return s.metadata_valid && upgrade_metadata_get()->pending.size == 0U;
}

bool srv_upgrade_active(void)
{
    return s.state != IDLE;
}

void srv_upgrade_on_frame(const drv_can_msg_t *msg, uint32_t now)
{
    if (msg->extended || msg->id != BOARD_CAN_UPGRADE_HOST_ID ||
        msg->dlc == 0U) {
        return;
    }
    const uint8_t cmd = msg->data[0];
    if (cmd == GET_INFO && msg->dlc >= 2U) {
        send_info(msg->data[1]);
        return;
    }
    if (s.state == RESET_PENDING) {
        return;
    }
    const uint8_t status = msg->dlc >= 2U ? handle_command(msg) : INVALID_FRAME;
    s.last_frame_ms = now;
    if (cmd == DATA && status == OK) {
        return;
    }
    const uint16_t index = (cmd == DATA_START && status == OK) ||
        status == BLOCK_INDEX_MISMATCH || status == INVALID_FRAME ? s.next_block : 0;
    const bool queued = reply(cmd, status, index);
    if (s.state == RESET_PENDING) {
        s.reboot_ack_queued = queued;
    }
}

bool srv_upgrade_step(uint32_t now)
{
    const uint32_t elapsed = now - s.last_frame_ms;
    if (s.state == RESET_PENDING) {
        if (!s.reboot_ack_queued) {
            s.reboot_ack_queued = reply(REBOOT, OK, 0);
        }
        return (s.reboot_ack_queued && drv_can_tx_idle()) || elapsed >= REBOOT_TIMEOUT_MS;
    }
    if (s.state != IDLE && elapsed >= SESSION_TIMEOUT_MS) {
        s.state = IDLE;
    } else if (s.state == RECEIVING && s.block_active && elapsed >= BLOCK_TIMEOUT_MS) {
        s.block_active = false;
        (void)reply(DATA_END, TIMEOUT, s.next_block);
    }
    return false;
}
