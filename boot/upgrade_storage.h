#ifndef UPGRADE_STORAGE_H
#define UPGRADE_STORAGE_H

#include "flash_layout.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t size;
    uint32_t crc;
    uint32_t version;
} upgrade_image_t;

typedef enum {
    UPGRADE_RESULT_NONE = 0,
    UPGRADE_RESULT_SUCCESS,
    UPGRADE_RESULT_FAILED,
} upgrade_result_t;

typedef enum {
    UPGRADE_INSTALL_OK = 0,
    UPGRADE_INSTALL_INVALID_IMAGE,
    UPGRADE_INSTALL_FLASH_ERROR,
} upgrade_install_error_t;

typedef struct {
    uint32_t reboot_count;
    uint16_t last_result;
    uint16_t last_error;
} upgrade_stats_t;

typedef struct {
    upgrade_image_t active;
    upgrade_image_t pending;
    upgrade_stats_t stats;
} upgrade_metadata_t;

/* B contains an A-linked image, so both vector tables are checked against A. */
bool upgrade_image_header_valid(const uint8_t *image, uint32_t size);
bool upgrade_image_read_version(const uint8_t *image, uint32_t size, uint32_t *version);
bool upgrade_metadata_init(void);
const upgrade_metadata_t *upgrade_metadata_get(void);
uint32_t upgrade_metadata_revision(void);
bool upgrade_metadata_save(const upgrade_metadata_t *next);

#endif
