#ifndef SRV_UPGRADE_H
#define SRV_UPGRADE_H

#include "drv_can.h"

bool srv_upgrade_init(void);
bool srv_upgrade_active(void);
void srv_upgrade_on_frame(const drv_can_msg_t *msg, uint32_t now);
/* Returns true when the reboot ACK has drained or the TX deadline has elapsed. */
bool srv_upgrade_step(uint32_t now);

#endif
