#ifndef CLAW_MAIN_H
#define CLAW_MAIN_H
#include "gd32f50x.h"
#include <stdint.h>
uint32_t board_millis(void);
void board_delay_ms(uint32_t ms);
void board_init(void);
void board_install_upgrade(uint16_t light_target);
void Error_Handler(void);
#endif
