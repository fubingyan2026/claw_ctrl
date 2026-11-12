#include "main.h"
#include "drv_fill_light.h"
#include "flash_layout.h"
#include "image_jump.h"

static volatile uint32_t s_ticks;
void SysTick_Handler(void) { ++s_ticks; }
uint32_t board_millis(void) { return s_ticks; }
void board_delay_ms(uint32_t ms) {
    uint32_t start = board_millis();
    while ((uint32_t)(board_millis()-start) < ms) { __WFI(); }
}
void board_init(void) {
    extern const uint32_t __gVectors[];
    SCB->VTOR = (uint32_t)__gVectors;
    __DSB();
    __ISB();
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

static bool boot_supports_handoff(void)
{
    const uint32_t *vectors = (const uint32_t *)BOOT_BASE;
    const uint32_t *header = (const uint32_t *)(BOOT_BASE + APP_HEADER_OFFSET);
    const uint32_t entry = vectors[1] & ~1U;
    return header[0] == BOOT_HANDOFF_MAGIC && header[1] == BOOT_HANDOFF_VERSION &&
        vectors[0] > 0x20000000U && vectors[0] <= 0x20010000U && (vectors[0] & 7U) == 0U &&
        (vectors[1] & 1U) != 0U && entry >= BOOT_BASE + APP_MIN_SIZE && entry < APP_A_BASE;
}

void board_install_upgrade(uint16_t light_target)
{
    drv_fill_light_save_for_upgrade(light_target);
    if (!boot_supports_handoff()) {
        NVIC_SystemReset();
    }

    __disable_irq();
    /* Stop RAM access by the old App's DMA and retire its interrupt sources. */
    if ((RCU_APB2EN & RCU_APB2EN_CAN0EN) != 0U) {
        gpio_bit_set(GPIOA, GPIO_PIN_10);
        can_deinit(CAN0);
    }
    if ((RCU_AHBEN & RCU_AHBEN_DMA0EN) != 0U) {
        usart_dma_transmit_config(USART0, USART_DENT_DISABLE);
        dma_channel_disable(DMA0, DMA_CH3);
    }
    SysTick->CTRL = 0;
    SysTick->VAL = 0;
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;
    const uint32_t nvic_banks = (SCnSCB->ICTR & SCnSCB_ICTR_INTLINESNUM_Msk) + 1U;
    for (uint32_t i = 0; i < nvic_banks; ++i) {
        NVIC->ICER[i] = UINT32_MAX;
        NVIC->ICPR[i] = UINT32_MAX;
    }
    __DSB();
    __ISB();
    image_jump(BOOT_BASE);
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
