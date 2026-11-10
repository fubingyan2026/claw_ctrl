#ifndef SRV_BOARD_PROTOCOL_H
#define SRV_BOARD_PROTOCOL_H

#include "board_protocol.h"
#include "drv_can.h"

/* Main-loop only. Periodic schedule is independent of immediate command reports. */
void srv_board_protocol_init(uint32_t now_ms);
bool srv_board_protocol_on_frame(const drv_can_msg_t *msg, uint32_t now_ms);
void srv_board_protocol_step(uint32_t now_ms);

#endif
