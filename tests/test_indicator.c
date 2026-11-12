#include "drv_rgb_led.h"
#include "srv_indicator.h"
#include <assert.h>
#include <stdio.h>

static uint16_t red, green, blue;

void drv_rgb_led_set(uint16_t r, uint16_t g, uint16_t b)
{
    red = r;
    green = g;
    blue = b;
}

static void check(juxie_aggregate_t request, uint32_t now, juxie_aggregate_t displayed,
    uint16_t r, uint16_t g, uint16_t b)
{
    assert(srv_indicator_update(request, now) == displayed);
    assert(red == r && green == g && blue == b);
}

static void test_startup(void)
{
    const uint32_t start = 100U;
    const uint32_t deadline = start + JUXIE_LINK_TIMEOUT_MS;
    srv_juxie_init(start);
    srv_indicator_init(start);
    srv_juxie_tick(start);
    check(srv_juxie_get_aggregate(), start, JUXIE_AGG_WAITING, 0, 0, 0);
    srv_juxie_tick(deadline);
    check(srv_juxie_get_aggregate(), deadline, JUXIE_AGG_WAITING, 0, 0, 0);
    srv_juxie_tick(deadline + 1U);
    check(srv_juxie_get_aggregate(), deadline + 1U, JUXIE_AGG_FAULT, IND_LEVEL_RED, 0, 0);

    srv_juxie_init(start);
    srv_indicator_init(start);
    check(srv_juxie_get_aggregate(), start, JUXIE_AGG_WAITING, 0, 0, 0);
    const uint32_t now = start + 100U;
    drv_can_msg_t feedback = {.id = JUXIE_FEEDBACK_BASE | JUXIE_DEV_ID_PRIMARY, .fd = true, .dlc = 12};
    srv_juxie_on_frame(&feedback, now);
    feedback.id = JUXIE_FEEDBACK_BASE | JUXIE_DEV_ID_MIRROR;
    srv_juxie_on_frame(&feedback, now);
    srv_juxie_tick(now);
    check(srv_juxie_get_aggregate(), now, JUXIE_AGG_DISABLED, 0, IND_LEVEL_GREEN, 0);
}

int main(void)
{
    test_startup();
    srv_indicator_init(0);
    check(JUXIE_AGG_UPGRADE, 10, JUXIE_AGG_UPGRADE, 0, 0, IND_LEVEL_BLUE);
    check(JUXIE_AGG_FAULT, 509, JUXIE_AGG_UPGRADE, 0, 0, IND_LEVEL_BLUE);
    check(JUXIE_AGG_FAULT, 510, JUXIE_AGG_UPGRADE, 0, 0, 0);
    check(JUXIE_AGG_FAULT, 1009, JUXIE_AGG_UPGRADE, 0, 0, 0);
    check(JUXIE_AGG_FAULT, 1010, JUXIE_AGG_FAULT, IND_LEVEL_RED, 0, 0);

    /* A late tick starts the off phase when the lamp actually turns off. */
    check(JUXIE_AGG_DISABLED, 1020, JUXIE_AGG_DISABLED, 0, IND_LEVEL_GREEN, 0);
    check(JUXIE_AGG_UPGRADE, 2200, JUXIE_AGG_DISABLED, 0, 0, 0);
    check(JUXIE_AGG_UPGRADE, 2699, JUXIE_AGG_DISABLED, 0, 0, 0);
    check(JUXIE_AGG_UPGRADE, 2700, JUXIE_AGG_UPGRADE, 0, 0, IND_LEVEL_BLUE);

    /* A stop request in a later cycle must also wait for that cycle's end. */
    check(JUXIE_AGG_UPGRADE, 3200, JUXIE_AGG_UPGRADE, 0, 0, 0);
    check(JUXIE_AGG_UPGRADE, 3700, JUXIE_AGG_UPGRADE, 0, 0, IND_LEVEL_BLUE);
    check(JUXIE_AGG_OFF, 4200, JUXIE_AGG_UPGRADE, 0, 0, 0);
    check(JUXIE_AGG_OFF, 4700, JUXIE_AGG_OFF, 0, 0, 0);

    const uint32_t start = UINT32_MAX - 250U;
    srv_indicator_init(start);
    check(JUXIE_AGG_UPGRADE, start, JUXIE_AGG_UPGRADE, 0, 0, IND_LEVEL_BLUE);
    check(JUXIE_AGG_OFF, start + 500U, JUXIE_AGG_UPGRADE, 0, 0, 0);
    check(JUXIE_AGG_OFF, start + 1000U, JUXIE_AGG_OFF, 0, 0, 0);
    puts("PASS: startup indicator, complete blink cycles, delayed ticks, color changes and tick rollover");
    return 0;
}
