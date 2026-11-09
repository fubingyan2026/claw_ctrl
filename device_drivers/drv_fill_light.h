#ifndef DRV_FILL_LIGHT_H
#define DRV_FILL_LIGHT_H
#include <stdint.h>
/* PA6 / TIMER2_CH0, active high, 0..1000. Default off. */
void drv_fill_light_init(void);
void drv_fill_light_set(uint16_t level);
#endif
