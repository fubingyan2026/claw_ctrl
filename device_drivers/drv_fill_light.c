#include "drv_fill_light.h"
#include "main.h"
void drv_fill_light_init(void)
{
    timer_parameter_struct base;
    timer_oc_parameter_struct oc;
    rcu_periph_clock_enable(RCU_TIMER2);
    gpio_bit_reset(GPIOA, GPIO_PIN_6);
    gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_6);
    timer_deinit(TIMER2);
    timer_struct_para_init(&base);
    base.prescaler = SystemCoreClock / 1000000U - 1U;
    base.period = 999U;
    timer_init(TIMER2, &base);
    timer_channel_output_struct_para_init(&oc);
    oc.outputstate = TIMER_CCX_ENABLE;
    oc.ocpolarity = TIMER_OC_POLARITY_HIGH;
    timer_channel_output_config(TIMER2, TIMER_CH_0, &oc);
    timer_channel_output_pulse_value_config(TIMER2, TIMER_CH_0, 0U);
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
    timer_channel_output_pulse_value_config(TIMER2, TIMER_CH_0, level > 1000U ? 1000U : level);
}
