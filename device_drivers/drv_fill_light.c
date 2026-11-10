#include "drv_fill_light.h"
#include "main.h"
static uint16_t s_level;
static uint32_t s_period_ticks;
void drv_fill_light_init(void)
{
    timer_parameter_struct base;
    timer_oc_parameter_struct oc;
    rcu_periph_clock_enable(RCU_TIMER2);
    gpio_bit_reset(GPIOA, GPIO_PIN_6);
    gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_6);
    timer_deinit(TIMER2);
    timer_struct_para_init(&base);
    /* TIMER2 clock = SystemCoreClock with the current APB1 timer clock setup.
     * At 252 MHz: PSC=0, ARR=50399, exactly 5 kHz (200 us per period).
     * Keeping the old 1 MHz counter would leave only 200 duty steps. */
    s_period_ticks = SystemCoreClock / DRV_FILL_LIGHT_PWM_HZ;
    base.prescaler = 1U;
    base.period = s_period_ticks - 1U;
    timer_init(TIMER2, &base);
    timer_channel_output_struct_para_init(&oc);
    oc.outputstate = TIMER_CCX_ENABLE;
    oc.ocpolarity = TIMER_OC_POLARITY_HIGH;
    timer_channel_output_config(TIMER2, TIMER_CH_0, &oc);
    timer_channel_output_pulse_value_config(TIMER2, TIMER_CH_0, 0U);
    s_level = 0U;
    timer_channel_output_mode_config(TIMER2, TIMER_CH_0, TIMER_OC_MODE_PWM0);
    timer_channel_output_shadow_config(TIMER2, TIMER_CH_0, TIMER_OC_SHADOW_ENABLE);
    timer_auto_reload_shadow_enable(TIMER2);
    timer_event_software_generate(TIMER2, TIMER_EVENT_SRC_UPG);
    timer_enable(TIMER2);
    gpio_af_set(GPIOA, GPIO_AF_1, GPIO_PIN_6);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_LEVEL1, GPIO_PIN_6);
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_6);
}
void drv_fill_light_set(uint16_t level)
{
    s_level = level > DRV_FILL_LIGHT_LEVEL_MAX ? DRV_FILL_LIGHT_LEVEL_MAX : level;
    uint32_t pulse = ((uint32_t)s_level * s_period_ticks + DRV_FILL_LIGHT_LEVEL_MAX / 2U)
                     / DRV_FILL_LIGHT_LEVEL_MAX;
    timer_channel_output_pulse_value_config(TIMER2, TIMER_CH_0, pulse);
}
uint16_t drv_fill_light_get(void) { return s_level; }
