#ifndef DRV_FLASH_H
#define DRV_FLASH_H

#include <stddef.h>
#include <stdint.h>

int drv_flash_read(uint32_t addr, uint8_t *data, size_t size);
int drv_flash_write(uint32_t addr, const uint8_t *data, size_t size);
int drv_flash_erase(uint32_t addr, size_t size);

#endif
