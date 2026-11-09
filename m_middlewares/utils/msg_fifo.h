#ifndef MSG_FIFO_H
#define MSG_FIFO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 嵌入式优化版消息 FIFO（基于槽位管理）
 */
typedef struct {
    uint8_t* buffer; /**< 存储区首地址 */
    uint32_t mask; /**< 槽位掩码（容量 - 1，容量必须为 2 的幂） */
    volatile uint32_t in; /**< 入队槽位索引（单调递增）；可能被中断/另一上下文更新 */
    volatile uint32_t out; /**< 出队槽位索引（单调递增）；可能被中断/另一上下文更新 */
    uint32_t elem_size; /**< 单个元素字节大小 */
} msg_fifo_t;

/* 宏辅助：声明一个固定容量的消息队列内存（容量必须是 2 的幂） */
#define MSG_FIFO_DEFINE_BUFFER(name, elem_type, count)          \
    static elem_type name##_buf[(count)];                       \
    static const uint32_t name##_elem_size = sizeof(elem_type); \
    static const uint32_t name##_capacity = (count)

/**
 * @brief 初始化 FIFO
 * @param fifo      控制块
 * @param buffer    外部分配的数组首地址
 * @param capacity  元素总容量（必须为 2 的幂，如 4, 8, 16, 32...）
 * @param elem_size 单个元素的字节数
 */
bool msg_fifo_init(msg_fifo_t* fifo, void* buffer, uint32_t capacity, uint32_t elem_size);

/* 基础出入队接口 */
bool msg_fifo_push(msg_fifo_t* fifo, const void* element);
bool msg_fifo_pop(msg_fifo_t* fifo, void* element);
bool msg_fifo_peek(const msg_fifo_t* fifo, void* element);

/* 零拷贝进阶接口（极致性能） */
void* msg_fifo_acquire_write(msg_fifo_t* fifo); /**< 获取当前可写槽位指针（若队列满返回 NULL） */
void msg_fifo_commit_write(msg_fifo_t* fifo); /**< 填充完成后，提交写入 */
void* msg_fifo_acquire_read(msg_fifo_t* fifo); /**< 获取当前可读槽位指针（若队列空返回 NULL） */
void msg_fifo_commit_read(msg_fifo_t* fifo); /**< 处理完成后，提交释放 */

/* 状态查询接口 */
static inline bool msg_fifo_is_empty(const msg_fifo_t* fifo)
{
    return fifo ? (fifo->in == fifo->out) : true;
}

static inline bool msg_fifo_is_full(const msg_fifo_t* fifo)
{
    return fifo ? ((fifo->in - fifo->out) > fifo->mask) : true;
}

static inline uint32_t msg_fifo_len(const msg_fifo_t* fifo)
{
    return fifo ? (fifo->in - fifo->out) : 0;
}

static inline uint32_t msg_fifo_avail(const msg_fifo_t* fifo)
{
    return fifo ? ((fifo->mask + 1) - (fifo->in - fifo->out)) : 0;
}

void msg_fifo_reset(msg_fifo_t* fifo);

#ifdef __cplusplus
}
#endif

#endif /* MSG_FIFO_H */