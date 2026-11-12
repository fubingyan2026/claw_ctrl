#include "drv_flash.h"
#include "flash_layout.h"
#include "gd32f50x.h"
#include <stdbool.h>
#include <string.h>

/* Both images and metadata are on the 512 KiB Bank 0. Protect Boot from writes. */
static bool valid_range(uint32_t addr, size_t size)
{
    const uint32_t end = METADATA_BASE + METADATA_SIZE;
    return addr >= APP_A_BASE && addr < end && size <= end - addr;
}

int drv_flash_read(uint32_t addr, uint8_t *data, size_t size)
{
    if (!valid_range(addr, size)) {
        return -1;
    }
    memcpy(data, (const void *)addr, size);
    return 0;
}

static void begin_flash_operation(void)
{
    fmc_unlock();
    fmc_flag_clear(FMC_FLAG_BANK0_PGERR);
    fmc_flag_clear(FMC_FLAG_BANK0_WPERR);
    fmc_flag_clear(FMC_FLAG_BANK0_END);
}

int drv_flash_write(uint32_t addr, const uint8_t *data, size_t size)
{
    if (!valid_range(addr, size) || ((addr | size) & 3U)) {
        return -1;
    }

    int result = 0;
    begin_flash_operation();
    for (size_t offset = 0; offset < size; offset += sizeof(uint32_t)) {
        uint32_t word;
        memcpy(&word, data + offset, sizeof(word));
        if (word != UINT32_MAX && fmc_word_program(addr + offset, word) != FMC_READY) {
            result = -1;
            break;
        }
    }
    fmc_lock();
    return result;
}

int drv_flash_erase(uint32_t addr, size_t size)
{
    if (!valid_range(addr, size) || ((addr | size) & (FLASH_PAGE_SIZE - 1U))) {
        return -1;
    }

    int result = 0;
    begin_flash_operation();
    for (size_t offset = 0; offset < size; offset += FLASH_PAGE_SIZE) {
        if (fmc_page_erase(addr + offset) != FMC_READY) {
            result = -1;
            break;
        }
    }
    fmc_lock();
    return result;
}
