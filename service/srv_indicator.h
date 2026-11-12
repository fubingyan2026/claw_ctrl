/*
 * Copyright (c) 2026 E1_PRO 项目组
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file    srv_indicator.h
 * @brief   双电机汇总状态到 RGB 灯效的映射
 */

#ifndef __SRV_INDICATOR_H
#define __SRV_INDICATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "srv_juxie.h"

#define IND_BLINK_HALF_MS (500U)

#define IND_LEVEL_WHITE_R (1000U)
#define IND_LEVEL_WHITE_G (1000U)
#define IND_LEVEL_WHITE_B (1000U)
#define IND_LEVEL_RED (1000U)
#define IND_LEVEL_GREEN (1000U)
#define IND_LEVEL_BLUE (1000U)

void srv_indicator_init(uint32_t now_ms);
/* Returns the displayed state. Blink transitions wait for the off phase to finish. */
juxie_aggregate_t srv_indicator_update(juxie_aggregate_t state, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* __SRV_INDICATOR_H */
