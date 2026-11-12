#include "drv_fill_light.h"
#include "gd32f50x.h"

#define LIGHT_UPGRADE_RESET 0xC1A7U
#define LIGHT_BOOT_HANDOFF  0xC1A8U
#define FILL_LIGHT_PRESCALER 1U

static uint16_t s_level;
static uint32_t s_period_ticks;

/* BKP_DATA0..2 hold the one-shot upgrade handoff: marker, applied duty, target. */
static void backup_write(uint16_t marker, uint16_t level, uint16_t target)
{
    rcu_periph_clock_enable(RCU_PMU);
    const bool was_writable = (PMU_CTL0 & PMU_CTL0_BKPWEN) != 0U;
    PMU_CTL0 |= PMU_CTL0_BKPWEN;
    BKP_DATA0 = 0;
    if (marker != 0U) {
        BKP_DATA1 = level;
        BKP_DATA2 = target;
        BKP_DATA0 = marker;
    }
    if (!was_writable) {
        PMU_CTL0 &= ~PMU_CTL0_BKPWEN;
    }
}

static bool read_backup(uint16_t marker, uint16_t *level, uint16_t *target)
{
    if (BKP_DATA0 != marker) {
        return false;
    }
    *level = BKP_DATA1;
    *target = BKP_DATA2;
    return *level <= DRV_FILL_LIGHT_LEVEL_MAX && *target <= DRV_FILL_LIGHT_LEVEL_MAX &&
        (*target != 0U || *level == 0U);
}

static bool read_running_pwm(uint16_t marker, uint16_t *level, uint16_t *target)
{
    const uint32_t clocks = RCU_APB1EN_BKPIEN | RCU_APB1EN_TIMER2EN;
    return (RCU_APB1EN & clocks) == clocks && read_backup(marker, level, target) &&
        (TIMER_CTL0(TIMER2) & TIMER_CTL0_CEN) != 0U &&
        TIMER_PSC(TIMER2) == FILL_LIGHT_PRESCALER &&
        TIMER_CAR(TIMER2) + 1U == SystemCoreClock / DRV_FILL_LIGHT_PWM_HZ &&
        rcu_clock_freq_get(CK_AHB) == SystemCoreClock;
}

void __real_SystemInit(void);
void __wrap_SystemInit(void)
{
    /* Both images use the same clock tree. Avoid relocking the PLL on handoff. */
    uint16_t level, target;
    if (!read_running_pwm(LIGHT_UPGRADE_RESET, &level, &target) &&
        !read_running_pwm(LIGHT_BOOT_HANDOFF, &level, &target)) {
        __real_SystemInit();
    }
}

static bool is_upgrade_reset(void)
{
    const uint32_t mask = RCU_RSTSCK_SWRSTF | RCU_RSTSCK_PORRSTF | RCU_RSTSCK_FWDGTRSTF |
        RCU_RSTSCK_WWDGTRSTF | RCU_RSTSCK_LPRSTF;
    return (RCU_RSTSCK & mask) == RCU_RSTSCK_SWRSTF;
}

static void fill_light_pin_low(void)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    gpio_bit_reset(GPIOA, GPIO_PIN_6);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_LEVEL1, GPIO_PIN_6);
    gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_6);
}

static void start_pwm(uint16_t level)
{
    timer_parameter_struct base;
    timer_oc_parameter_struct oc;
    fill_light_pin_low();
    rcu_periph_clock_enable(RCU_TIMER2);
    timer_deinit(TIMER2);
    timer_struct_para_init(&base);
    s_period_ticks = SystemCoreClock / DRV_FILL_LIGHT_PWM_HZ;
    base.prescaler = FILL_LIGHT_PRESCALER;
    base.period = s_period_ticks - 1U;
    timer_init(TIMER2, &base);
    timer_channel_output_struct_para_init(&oc);
    oc.outputstate = TIMER_CCX_ENABLE;
    oc.ocpolarity = TIMER_OC_POLARITY_HIGH;
    timer_channel_output_config(TIMER2, TIMER_CH_0, &oc);
    timer_channel_output_mode_config(TIMER2, TIMER_CH_0, TIMER_OC_MODE_PWM0);
    timer_channel_output_shadow_config(TIMER2, TIMER_CH_0, TIMER_OC_SHADOW_ENABLE);
    timer_auto_reload_shadow_enable(TIMER2);
    drv_fill_light_set(level);
    timer_event_software_generate(TIMER2, TIMER_EVENT_SRC_UPG);
    timer_enable(TIMER2);
    gpio_af_set(GPIOA, GPIO_AF_1, GPIO_PIN_6);
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_6);
}

static void adopt_pwm(uint16_t level)
{
    s_period_ticks = TIMER_CAR(TIMER2) + 1U;
    s_level = level;
}

void drv_fill_light_save_for_upgrade(uint16_t target)
{
    rcu_periph_clock_enable(RCU_BKPI);
    rcu_all_reset_flag_clear();
    backup_write(LIGHT_UPGRADE_RESET, s_level, target);
}

bool drv_fill_light_boot_restore(void)
{
    rcu_periph_clock_enable(RCU_BKPI);
    uint16_t level, target;
    const bool running = read_running_pwm(LIGHT_UPGRADE_RESET, &level, &target);
    const bool restore = running || (is_upgrade_reset() && read_backup(LIGHT_UPGRADE_RESET, &level, &target));
    if (restore) {
        if (running) {
            adopt_pwm(level);
        } else {
            start_pwm(level);
        }
        backup_write(LIGHT_BOOT_HANDOFF, level, target);
    } else {
        fill_light_pin_low();
        if (BKP_DATA0 != 0U) {
            backup_write(0, 0, 0);
        }
    }
    return restore;
}

uint16_t drv_fill_light_init(void)
{
    rcu_periph_clock_enable(RCU_BKPI);
    uint16_t level = 0, target = 0;
    if (read_running_pwm(LIGHT_BOOT_HANDOFF, &level, &target)) {
        adopt_pwm(level);
    } else {
        /* An older Boot can leave the request untouched. A changed clock
         * can also require reinitializing a PWM started by Boot. */
        if (!is_upgrade_reset() || (!read_backup(LIGHT_UPGRADE_RESET, &level, &target) &&
            !read_backup(LIGHT_BOOT_HANDOFF, &level, &target))) {
            level = target = 0;
        }
        start_pwm(level);
    }
    if (BKP_DATA0 != 0U) {
        backup_write(0, 0, 0);
    }
    return target;
}

void drv_fill_light_set(uint16_t level)
{
    s_level = level > DRV_FILL_LIGHT_LEVEL_MAX ? DRV_FILL_LIGHT_LEVEL_MAX : level;
    uint32_t pulse = ((uint32_t)s_level * s_period_ticks + DRV_FILL_LIGHT_LEVEL_MAX / 2U)
                     / DRV_FILL_LIGHT_LEVEL_MAX;
    timer_channel_output_pulse_value_config(TIMER2, TIMER_CH_0, pulse);
}
uint16_t drv_fill_light_get(void) { return s_level; }
