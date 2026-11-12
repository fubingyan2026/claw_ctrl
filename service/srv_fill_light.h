#ifndef SRV_FILL_LIGHT_H
#define SRV_FILL_LIGHT_H
#include <stdint.h>
#define FILL_LIGHT_FILTER_PERIOD_MS 10U
#define FILL_LIGHT_FILTER_TAU_MS 100U
void srv_fill_light_init(uint32_t now_ms, uint16_t target);
void srv_fill_light_set_target(uint16_t level, uint32_t now_ms);
uint16_t srv_fill_light_get_target(void);
void srv_fill_light_step(uint32_t now_ms);
#endif
