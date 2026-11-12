/*
 * Copyright (c) 2026 E1_PRO 项目组
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file    drv_can.h
 * @brief   CAN0 设备驱动（GD32F503，CAN FD 1M/1M NO_BRS）
 * @note    RX/TX 均通过 msg_fifo 缓冲：
 *          - RX：FDCAN 中断只负责把报文压入 RX FIFO，主循环 drv_can_recv() 取出处理
 *          - TX：drv_can_send() 只入队，drv_can_poll() 在主循环中提交到硬件
 */

#ifndef __DRV_CAN_H
#define __DRV_CAN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief CAN 报文（dlc 为实际字节数 0..64）
 */
typedef struct {
    uint32_t id;
    bool extended;
    bool fd;
    uint8_t dlc;
    uint8_t data[64];
} drv_can_msg_t;

/** @brief RX 队列深度（元素个数，必须是 2 的幂，传给 msg_fifo_init 的 capacity） */
#define DRV_CAN_RX_QUEUE_DEPTH (32U)
/** @brief TX 队列深度（元素个数，必须是 2 的幂，传给 msg_fifo_init 的 capacity） */
#define DRV_CAN_TX_QUEUE_DEPTH (16U)

/** @brief RX 队列实际内存字节数 = 元素大小 × 队列深度 */
#define DRV_CAN_RX_QUEUE_BYTES (sizeof(drv_can_msg_t) * DRV_CAN_RX_QUEUE_DEPTH)
/** @brief TX 队列实际内存字节数 = 元素大小 × 队列深度 */
#define DRV_CAN_TX_QUEUE_BYTES (sizeof(drv_can_msg_t) * DRV_CAN_TX_QUEUE_DEPTH)

/** @brief 初始化 CAN0、滤波器和 RX/TX 消息队列 */
bool drv_can_init(void);

/** @brief 发送报文（仅入 TX 队列，不直接写硬件） */
bool drv_can_send(const drv_can_msg_t* msg);

/** @brief 发送同步帧 0x80（CAN FD，0 字节，NO_BRS，仅入队） */
bool drv_can_send_sync(void);

/** @brief 从 RX 队列取出一帧（主循环上下文） */
bool drv_can_recv(drv_can_msg_t* msg);

/** @brief 主循环周期调用：提交 TX 队列、检测 Bus-Off 并恢复 */
void drv_can_poll(void);

/** @brief TX 队列和硬件发送邮箱均已空闲 */
bool drv_can_tx_idle(void);

/** @brief RX 队列溢出丢帧计数 */
uint32_t drv_can_get_rx_dropped(void);

/** @brief TX 队列提交失败丢帧计数 */
uint32_t drv_can_get_tx_dropped(void);

#ifdef __cplusplus
}
#endif

#endif /* __DRV_CAN_H */
