/* GD32F503, schematic: PA1=R, PA3=G, PA2=B. Common anode. */
#include "drv_rgb_led.h"
#include "main.h"
static uint16_t clamp(uint16_t v) { return v > 1000U ? 1000U : v; }
void drv_rgb_led_init(void)
{
    const uint32_t pins = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3;
    const uint16_t channels[] = { TIMER_CH_1, TIMER_CH_2, TIMER_CH_3 };
    timer_parameter_struct base;
    timer_oc_parameter_struct oc;
    rcu_periph_clock_enable(RCU_TIMER1);
    gpio_bit_set(GPIOA, pins);
    gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, pins);
    timer_deinit(TIMER1);
    timer_struct_para_init(&base);
    /* APB1=126MHz, timer clock=252MHz with vendor default clock tree. */
    base.prescaler = SystemCoreClock / 1000000U - 1U;
    base.period = 999U;
    timer_init(TIMER1, &base);
    timer_channel_output_struct_para_init(&oc);
    oc.outputstate = TIMER_CCX_ENABLE;
    oc.ocpolarity = TIMER_OC_POLARITY_HIGH;
    for (unsigned i = 0; i < 3; ++i) {
        timer_channel_output_config(TIMER1, channels[i], &oc);
        timer_channel_output_pulse_value_config(TIMER1, channels[i], 1000U);
        timer_channel_output_mode_config(TIMER1, channels[i], TIMER_OC_MODE_PWM0);
        timer_channel_output_shadow_config(TIMER1, channels[i], TIMER_OC_SHADOW_ENABLE);
    }
    timer_auto_reload_shadow_enable(TIMER1);
    timer_event_software_generate(TIMER1, TIMER_EVENT_SRC_UPG);
    timer_enable(TIMER1);
    gpio_af_set(GPIOA, GPIO_AF_1, pins);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_LEVEL1, pins);
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, pins);
}
void drv_rgb_led_set(uint16_t r, uint16_t g, uint16_t b)
{
    timer_channel_output_pulse_value_config(TIMER1, TIMER_CH_1, 1000U - clamp(r));
    timer_channel_output_pulse_value_config(TIMER1, TIMER_CH_3, 1000U - clamp(g));
    timer_channel_output_pulse_value_config(TIMER1, TIMER_CH_2, 1000U - clamp(b));
}
