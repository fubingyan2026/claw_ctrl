#ifndef IMAGE_JUMP_H
#define IMAGE_JUMP_H

#include "gd32f50x.h"

/* Called from thread mode with SysTick and peripheral IRQs stopped. */
static inline _Noreturn void image_jump(uint32_t address)
{
    const uint32_t *vectors = (const uint32_t *)address;
    SCB->VTOR = address;
    __DSB();
    __ISB();
    __asm volatile ("msr msp, %0\n\tcpsie i\n\tbx %1" :: "r"(vectors[0]), "r"(vectors[1]) : "memory");
    __builtin_unreachable();
}

#endif
