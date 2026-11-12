#include "drv_flash.h"
#include "drv_fill_light.h"
#include "image_jump.h"
#include "rs_crc32.h"
#include "upgrade_storage.h"

__attribute__((used, section(".boot_header")))
static const uint32_t boot_header[] = { BOOT_HANDOFF_MAGIC, BOOT_HANDOFF_VERSION };

static bool valid_image(uint32_t base, const upgrade_image_t *image)
{
    const uint8_t *data = (const uint8_t *)base;
    return upgrade_image_header_valid(data, image->size) &&
        rs_crc32(data, image->size) == image->crc;
}

static bool promote_image(const upgrade_image_t *image)
{
    for (uint32_t offset = 0; offset < image->size; offset += FLASH_PAGE_SIZE) {
        uint32_t size = image->size - offset;
        if (size > FLASH_PAGE_SIZE) {
            size = FLASH_PAGE_SIZE;
        }
        if (drv_flash_erase(APP_A_BASE + offset, FLASH_PAGE_SIZE) != 0 ||
            drv_flash_write(APP_A_BASE + offset, (const uint8_t *)(APP_B_BASE + offset),
                (size + 3U) & ~3U) != 0) {
            return false;
        }
    }
    return valid_image(APP_A_BASE, image);
}

static void boot_led(uint32_t pin)
{
    const uint32_t rgb = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3;
    rcu_periph_clock_enable(RCU_GPIOA);
    gpio_bit_set(GPIOA, rgb);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_LEVEL1, rgb);
    gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, rgb);
    gpio_bit_reset(GPIOA, pin);
}

static bool select_app(upgrade_metadata_t *meta)
{
    if (meta->pending.size == 0U) {
        return valid_image(APP_A_BASE, &meta->active);
    }
    boot_led(GPIO_PIN_2);
    if (!valid_image(APP_A_BASE, &meta->pending)) {
        if (!valid_image(APP_B_BASE, &meta->pending)) {
            meta->stats.last_result = UPGRADE_RESULT_FAILED;
            meta->stats.last_error = UPGRADE_INSTALL_INVALID_IMAGE;
            if (!valid_image(APP_A_BASE, &meta->active)) {
                return false;
            }
            meta->pending = (upgrade_image_t){0};
            return true;
        }
        if (!promote_image(&meta->pending)) {
            meta->stats.last_result = UPGRADE_RESULT_FAILED;
            meta->stats.last_error = UPGRADE_INSTALL_FLASH_ERROR;
            return false;
        }
    }
    meta->active = meta->pending;
    meta->pending = (upgrade_image_t){0};
    meta->stats.last_result = UPGRADE_RESULT_SUCCESS;
    meta->stats.last_error = UPGRADE_INSTALL_OK;
    return true;
}

int main(void)
{
    const bool light_restored = drv_fill_light_boot_restore();
    if (upgrade_metadata_init()) {
        upgrade_metadata_t next = *upgrade_metadata_get();
        if (next.stats.reboot_count != UINT32_MAX) {
            ++next.stats.reboot_count;
        }
        const bool app_valid = select_app(&next);
        if (app_valid) {
            (void)upgrade_image_read_version((const uint8_t *)APP_A_BASE, next.active.size, &next.active.version);
        }
        (void)upgrade_metadata_save(&next);
        if (app_valid) {
            image_jump(APP_A_BASE);
        }
    }
    if (light_restored) {
        drv_fill_light_set(0);
    }
    boot_led(GPIO_PIN_1);
    for (;;) {
        __WFI();
    }
}
