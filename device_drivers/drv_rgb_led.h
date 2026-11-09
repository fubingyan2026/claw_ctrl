/*
 * Copyright (c) 2026 E1_PRO 项目组
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file    drv_rgb_led.h
 * @brief   共阳 RGB LED PWM 驱动（TIMER1_CH1 / CH2 / CH3, PA1 / PA2 / PA3）
 */

#ifndef __DRV_RGB_LED_H
#define __DRV_RGB_LED_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define DRV_RGB_LED_LEVEL_MAX (1000U)

/** @brief 初始化三路 PWM，默认全部熄灭 */
void drv_rgb_led_init(void);

/**
 * @brief 设置 RGB 亮度
 * @param r 红色 0..1000
 * @param g 绿色 0..1000
 * @param b 蓝色 0..1000
 */
void drv_rgb_led_set(uint16_t r, uint16_t g, uint16_t b);

#ifdef __cplusplus
}
#endif

#endif /* __DRV_RGB_LED_H */
