#include "main.h"
static volatile uint32_t s_ticks;
void SysTick_Handler(void) { ++s_ticks; }
uint32_t board_millis(void) { return s_ticks; }
void board_delay_ms(uint32_t ms) {
    uint32_t start = board_millis();
    while ((uint32_t)(board_millis()-start) < ms) { __WFI(); }
}
void board_init(void) {
    SystemCoreClockUpdate();
    nvic_priority_group_set(NVIC_PRIGROUP_PRE2_SUB2);
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    gpio_bit_set(GPIOA, GPIO_PIN_10);
    gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_10);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_LEVEL1, GPIO_PIN_10);
    if (SysTick_Config(SystemCoreClock/1000U)) { Error_Handler(); }
    NVIC_SetPriority(SysTick_IRQn, 3U);
}
void Error_Handler(void) {
    __disable_irq();
    gpio_bit_set(GPIOA, GPIO_PIN_10);
    for (;;) { __NOP(); }
}
void HardFault_Handler(void) { Error_Handler(); }
void MemManage_Handler(void) { Error_Handler(); }
void BusFault_Handler(void) { Error_Handler(); }
void UsageFault_Handler(void) { Error_Handler(); }
