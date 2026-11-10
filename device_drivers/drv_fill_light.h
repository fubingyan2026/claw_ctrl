#ifndef DRV_FILL_LIGHT_H
#define DRV_FILL_LIGHT_H
#include <stdint.h>
#define DRV_FILL_LIGHT_LEVEL_MAX 1000U
#define DRV_FILL_LIGHT_PWM_HZ 5000U
/* PA6 / TIMER2_CH0, active high, 5 kHz. 0..1000 = 0..100% duty. Default off.
 * DIM is driven directly: the analog 1.1..3.1 V table does not apply to PWM. */
void drv_fill_light_init(void);
void drv_fill_light_set(uint16_t level);
/* Last applied duty in 0.1% units, not raw CCR or measured optical output. */
uint16_t drv_fill_light_get(void);
#endif
