#ifndef SRV_BOARD_PROTOCOL_H
#define SRV_BOARD_PROTOCOL_H

#include "board_protocol.h"
#include "drv_can.h"

/* Main-loop only. Returns true when a valid board command is handled. */
bool srv_board_protocol_on_frame(const drv_can_msg_t *msg, uint32_t now_ms);

#endif
