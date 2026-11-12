#include "upgrade_storage.h"
#include "drv_flash.h"
#include "ring_storage.h"
#include <string.h>

_Static_assert(offsetof(upgrade_metadata_t, stats) == 24U &&
    sizeof(upgrade_stats_t) == 8U, "Factory metadata format");
_Static_assert(APP_A_BASE % FLASH_PAGE_SIZE == 0U && APP_B_BASE % FLASH_PAGE_SIZE == 0U &&
    METADATA_BASE % FLASH_PAGE_SIZE == 0U && METADATA_SIZE >= 2U * FLASH_PAGE_SIZE,
    "Flash partitions must be page aligned");

static upgrade_metadata_t metadata;
static ring_storage_context_t storage;
static _Alignas(uint32_t) uint8_t frame[128];

/* Both callers use storage only from the main context. The FMC driver locks
 * the controller after each operation. */
static void storage_lock(void) {}

bool upgrade_image_header_valid(const uint8_t *image, uint32_t size)
{
    if (size < APP_MIN_SIZE || size > APP_SLOT_SIZE) {
        return false;
    }
    uint32_t vectors[2], header[2];
    memcpy(vectors, image, sizeof(vectors));
    memcpy(header, image + APP_HEADER_OFFSET, sizeof(header));
    const uint32_t entry = vectors[1] & ~1U;
    return vectors[0] > 0x20000000U && vectors[0] <= 0x20010000U &&
        (vectors[0] & 7U) == 0U && (vectors[1] & 1U) != 0U &&
        entry >= APP_A_BASE + APP_MIN_SIZE && entry < APP_A_BASE + size &&
        header[0] == APP_MAGIC && header[1] == APP_TARGET;
}

bool upgrade_image_read_version(const uint8_t *image, uint32_t size, uint32_t *version)
{
    uint32_t field[2];
    if (size < APP_MIN_SIZE + sizeof(field)) {
        return false;
    }
    memcpy(field, image + APP_MIN_SIZE, sizeof(field));
    if (field[0] != APP_VERSION_MAGIC || field[1] > 0xFFFFFFU) {
        return false;
    }
    *version = field[1];
    return true;
}

bool upgrade_metadata_init(void)
{
    const ring_storage_config_t config = {
        .port = { drv_flash_read, drv_flash_write, drv_flash_erase, storage_lock, storage_lock },
        .start_addr = METADATA_BASE,
        .area_size = METADATA_SIZE,
        .sector_size = FLASH_PAGE_SIZE,
        .write_gran = RING_STORAGE_WRITE_GRAN_32,
        .frame_buffer = frame,
        .frame_buffer_size = sizeof(frame),
    };
    memset(&metadata, 0, sizeof(metadata));
    return ring_storage_init(&storage, &config) == RING_STORAGE_OK &&
        ring_storage_register(&storage, "image", &metadata,
            offsetof(upgrade_metadata_t, stats)) == RING_STORAGE_OK &&
        ring_storage_register(&storage, "stats", &metadata.stats, sizeof(metadata.stats)) == RING_STORAGE_OK &&
        ring_storage_load(&storage) == RING_STORAGE_OK &&
        metadata.active.size >= APP_MIN_SIZE && metadata.active.size <= APP_SLOT_SIZE;
}

uint32_t upgrade_metadata_revision(void)
{
    return storage.latest_version;
}

const upgrade_metadata_t *upgrade_metadata_get(void)
{
    return &metadata;
}

bool upgrade_metadata_save(const upgrade_metadata_t *next)
{
    /* A reset during save can leave programmed words after the last committed
     * frame. Let ring_storage roll to the next page instead of rewriting them. */
    for (uint32_t offset = storage.write_offset; offset < FLASH_PAGE_SIZE; offset += 4U) {
        if (*(const uint32_t *)(storage.active_sector_addr + offset) != UINT32_MAX) {
            storage.write_offset = FLASH_PAGE_SIZE;
            break;
        }
    }

    const upgrade_metadata_t previous = metadata;
    metadata = *next;
    if (ring_storage_save(&storage) != RING_STORAGE_OK) {
        metadata = previous;
        return false;
    }
    return true;
}
