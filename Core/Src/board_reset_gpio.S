/* GD32F503 register addresses and fields used before C runtime initialization. */
.syntax unified
.thumb

.equ RCU,             0x40021000
.equ RCU_APB2EN,      0x18
.equ RCU_APB1EN,      0x1c
.equ GPIOAEN,         (1 << 2)
.equ TIMER2EN,        (1 << 1)
.equ GPIOA,           0x40010800
.equ GPIO_CTL,        0x00
.equ GPIO_OMODE,      0x04
.equ GPIO_BOP,        0x18
.equ RGB_PINS,        ((1 << 1) | (1 << 2) | (1 << 3))
.equ RGB_MODE_MASK,   ((3 << 2) | (3 << 4) | (3 << 6))
.equ RGB_MODE_OUTPUT, ((1 << 2) | (1 << 4) | (1 << 6))
.equ PA6,             (1 << 6)
.equ PA6_MODE_MASK,   (3 << 12)
.equ PA6_MODE_OUTPUT, (1 << 12)

.section .text.board_reset_gpio,"ax",%progbits
.global board_reset_gpio
.thumb_func
board_reset_gpio:
    /* TIMER2 stays clocked on an OTA handoff and resets off on hardware reset. */
    ldr r0, =RCU
    ldr r1, [r0, #RCU_APB1EN]
    tst r1, #TIMER2EN
    bne 1f

    ldr r1, [r0, #RCU_APB2EN]
    orr r1, r1, #GPIOAEN
    str r1, [r0, #RCU_APB2EN]
    ldr r1, [r0, #RCU_APB2EN]

    /* Preload RGB high and DIM low before enabling the outputs. */
    ldr r0, =GPIOA
    ldr r1, =(RGB_PINS | (PA6 << 16))
    str r1, [r0, #GPIO_BOP]
    ldr r1, [r0, #GPIO_OMODE]
    bic r1, r1, #(RGB_PINS | PA6)
    str r1, [r0, #GPIO_OMODE]
    ldr r1, [r0, #GPIO_CTL]
    bic r1, r1, #RGB_MODE_MASK
    bic r1, r1, #PA6_MODE_MASK
    orr r1, r1, #RGB_MODE_OUTPUT
    orr r1, r1, #PA6_MODE_OUTPUT
    str r1, [r0, #GPIO_CTL]
1:
    bx lr
.size board_reset_gpio, .-board_reset_gpio
