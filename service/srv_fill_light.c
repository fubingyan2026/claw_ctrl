#include "srv_fill_light.h"
#include "drv_fill_light.h"
#include "filter.h"
#include <stdbool.h>

static pt1Filter_t s_filter;
static uint16_t s_target;
static uint32_t s_last_ms;
static bool s_initialized;

void srv_fill_light_init(uint32_t now_ms, uint16_t target)
{
    pt1FilterInit(&s_filter, 0.0f);
    s_filter.state = (float)drv_fill_light_get();
    s_target = target;
    s_last_ms = now_ms;
    s_initialized = true;
}

void srv_fill_light_set_target(uint16_t level, uint32_t now_ms)
{
    if (!s_initialized) { return; }
    s_target = level > DRV_FILL_LIGHT_LEVEL_MAX ? DRV_FILL_LIGHT_LEVEL_MAX : level;
    if (s_target == 0) {
        s_filter.state = 0.0f;
        s_last_ms = now_ms;
        drv_fill_light_set(0); /* OFF bypasses filtering. */
    }
}

uint16_t srv_fill_light_get_target(void) { return s_target; }

void srv_fill_light_step(uint32_t now_ms)
{
    uint32_t elapsed = now_ms - s_last_ms;
    if (!s_initialized || elapsed < FILL_LIGHT_FILTER_PERIOD_MS) { return; }
    s_last_ms = now_ms;
    /* Account for scheduler jitter without running a burst of catch-up updates. */
    float dt = (float)elapsed;
    pt1FilterUpdateCutoff(&s_filter, dt / ((float)FILL_LIGHT_FILTER_TAU_MS + dt));
    float output = pt1FilterApply(&s_filter, (float)s_target);
    float error = output - (float)s_target;
    if (error < 0.5f && error > -0.5f) { s_filter.state = output = (float)s_target; }
    drv_fill_light_set((uint16_t)(output + 0.5f));
}
