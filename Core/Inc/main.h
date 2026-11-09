#ifndef CLAW_MAIN_H
#define CLAW_MAIN_H
#include "gd32f50x.h"
#include <stdint.h>
uint32_t board_millis(void);
void board_delay_ms(uint32_t ms);
void board_init(void);
void Error_Handler(void);
#endif
