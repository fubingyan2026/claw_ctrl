#include "msg_fifo.h"
#include <string.h>

/* 嵌入式屏障定义：兼顾 ARM Cortex-M CMSIS 与通用 GCC/Clang */
#if defined(__ARMCC_VERSION) || defined(__GNUC__)
/* 针对单核 MCU，主要是防止编译器乱序重排 */
#define EMB_COMPILER_BARRIER() __asm__ __volatile__("" ::: "memory")
/* 针对双核/SMP 或 Cortex-M7 双发射架构，使用硬件内存屏障 */
#if defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__) || defined(__ARM_ARCH_8M_MAIN__)
#define EMB_DATA_BARRIER() __asm__ __volatile__("dmb" ::: "memory")
#else
#define EMB_DATA_BARRIER() EMB_COMPILER_BARRIER()
#endif
#else
#define EMB_COMPILER_BARRIER() ((void)0)
#define EMB_DATA_BARRIER() ((void)0)
#endif

/* 检查是否为 2 的幂 */
static inline bool is_power_of_two(uint32_t val)
{
    return (val > 1) && ((val & (val - 1)) == 0);
}
/* 计算小于或等于 val 的最接近的 2 的幂（向下取整）
 * 例：
 *   100 -> 64
 *   10  -> 8
 *   8   -> 8 (本身是 2 的幂则保持不变)
 *   < 2 -> 0 (不满足最小 FIFO 要求)
 */
static inline uint32_t round_down_power_of_two(uint32_t val)
{
    if (val < 2) {
        return 0;
    }
    /* 位扩散算法（Bit-smearing）：将最高有效位之后的全填为 1 */
    val |= val >> 1;
    val |= val >> 2;
    val |= val >> 4;
    val |= val >> 8;
    val |= val >> 16;
    
    /* 此时 val 形式为 0...0111...1，如 10 变为 15 (0b1111) */
    /* 15 - (15 >> 1) = 15 - 7 = 8，成功提取出最高位 */
    return val - (val >> 1);
}

bool msg_fifo_init(msg_fifo_t* fifo, void* buffer, uint32_t capacity, uint32_t elem_size)
{
    if (!fifo || !buffer || elem_size == 0) {
        return false;
    }

    /* 兜底处理：自动将容量规整为小于等于当前值的最接近 2 的幂 */
    capacity = round_down_power_of_two(capacity);

    /* 若传入的 capacity 连 2 都不到（如 0 或 1），无法构成环形队列，依然返回 false */
    if (capacity < 2) {
        return false;
    }

    fifo->buffer = (uint8_t*)buffer;
    fifo->mask = capacity - 1; /* 2^N - 1 作为掩码 */
    fifo->in = 0;
    fifo->out = 0;
    fifo->elem_size = elem_size;

    return true;
}

void msg_fifo_reset(msg_fifo_t* fifo)
{
    if (fifo) {
        fifo->in = 0;
        fifo->out = 0;
    }
}

/* 基础入队：仅 1 次 memcpy */
bool msg_fifo_push(msg_fifo_t* fifo, const void* element)
{
    if (!fifo || !element) {
        return false;
    }

    /* 检查是否已满 */
    if ((fifo->in - fifo->out) > fifo->mask) {
        return false;
    }

    /* 定位槽位地址：纯位运算，无除法 */
    uint8_t* slot = fifo->buffer + ((fifo->in & fifo->mask) * fifo->elem_size);

    /* 始终 memcpy: 避免把未对齐/别名不兼容的内存强转成 uint32_t* (编译器会自动优化) */
    memcpy(slot, element, fifo->elem_size);

    /* 写屏障：确保数据已经完整落入 RAM，再更新 in 索引让消费者可见 */
    EMB_DATA_BARRIER();
    fifo->in++;

    return true;
}

/* 基础出队：仅 1 次 memcpy */
bool msg_fifo_pop(msg_fifo_t* fifo, void* element)
{
    if (!fifo || !element) {
        return false;
    }

    /* 检查是否为空 */
    if (fifo->in == fifo->out) {
        return false;
    }

    /* 读屏障：确保读取到合法的 in 后再进行数据搬移 */
    EMB_DATA_BARRIER();

    uint8_t* slot = fifo->buffer + ((fifo->out & fifo->mask) * fifo->elem_size);

    memcpy(element, slot, fifo->elem_size);

    /* 内存释放屏障：确保数据已被读取完成，再更新 out 释放槽位 */
    EMB_DATA_BARRIER();
    fifo->out++;

    return true;
}

bool msg_fifo_peek(const msg_fifo_t* fifo, void* element)
{
    if (!fifo || !element || (fifo->in == fifo->out)) {
        return false;
    }

    EMB_DATA_BARRIER();
    uint8_t* slot = fifo->buffer + ((fifo->out & fifo->mask) * fifo->elem_size);

    memcpy(element, slot, fifo->elem_size);

    return true;
}

/* ========== 零拷贝实现 ========== */

void* msg_fifo_acquire_write(msg_fifo_t* fifo)
{
    if (!fifo || ((fifo->in - fifo->out) > fifo->mask)) {
        return NULL; /* 队列已满 */
    }
    return fifo->buffer + ((fifo->in & fifo->mask) * fifo->elem_size);
}

void msg_fifo_commit_write(msg_fifo_t* fifo)
{
    if (fifo) {
        EMB_DATA_BARRIER();
        fifo->in++;
    }
}

void* msg_fifo_acquire_read(msg_fifo_t* fifo)
{
    if (!fifo || (fifo->in == fifo->out)) {
        return NULL; /* 队列为空 */
    }
    EMB_DATA_BARRIER();
    return fifo->buffer + ((fifo->out & fifo->mask) * fifo->elem_size);
}

void msg_fifo_commit_read(msg_fifo_t* fifo)
{
    if (fifo) {
        EMB_DATA_BARRIER();
        fifo->out++;
    }
}