# GD32F503 中间件导入与适配

来源：`C:/Users/32457/Desktop/R1_CTU_POWER/public_layer/m_middlewares`，2026-11-10。
按本次用户要求复制到当前独立工程，未修改来源工程。

## 导入范围

已复制 algorithm（CRC、PT1/biquad 等滤波器、PID/MIT、PLL、数学工具）、framework（状态机、事件、软件定时器、守护模块）、key_base、log、protocol_tools、utils 及 Third_Party。
第三方包括 mpaland_printf、SEGGER RTT、CmBacktrace、lwmem、ring_storage；版权声明和随附文档保留。
排除来源工程的 AGENTS.md、编译缓存 `.gch`、目标文件和 IDE `.uga` 文件。
来源文件及 SHA-256、目的路径、本地修改标志记录在 `IMPORT_MANIFEST.json`。

## CMake 接入

根工程通过 `add_subdirectory(m_middlewares)` 和 `target_link_libraries(claw_ctrl ... m_middlewares)` 使用静态库。
通用 C 模块参与编译，未被引用的函数和数据在链接时剔除；数学依赖链接 libm。
当前实际启用 UART 日志、消息队列、printf、PT1 补光灯滤波；其余通用模块可按需调用。
SEGGER RTT、CmBacktrace、lwmem、ring_storage 已复制但未加入默认固件构建。启用它们前需完成 GD32 的配置/异常入口/内存分配或 Flash 读写接口适配；尤其来源 CmBacktrace 配置选择 M4，不能直接用其异常汇编替换当前 M33 入口。
本次未启用动态分配、RTOS、Flash 持久化或 RTT 日志，未更改源工程。

## 同名文件兼容

- 现有 `utils/msg_fifo.*` 保留槽位队列语义：capacity 是元素个数，CAN RX/TX 深度仍为 32/16。
- 按用户要求不导入来源 `framework/msg_fifo.*`，仅保留现有 `utils/msg_fifo.*`；两者的容量单位和 ABI 不同。
- `kfifo.h` 在 Arm 编译时通过 CMSIS compiler 内建函数提供 PRIMASK 保存/恢复、中断临界区和 DMB；主机测试保留无硬件默认实现。
- 使用来源新版日志，保留新增输出暂停和 Flash sink 接口；修复配置应用/重复初始化，日志缓存保持本工程原有 4096 字节，保留 UART 输出行为。
- 协议解析器的帧头检查会丢弃错位字节，移除对应内部参数的错误 const 限定。

## 补光灯 PT1

`srv_fill_light` 使用本次导入的 `pt1FilterInit/pt1FilterUpdateCutoff/pt1FilterApply`。
正常约每 10ms 更新，时间常数 100ms，`k=dt/(100+dt)`，dt 为实际间隔（ms）。
亮度阶跃约 0.3s 接近变化量的 95%，差值小于 0.5/1000 时吸附到目标，避免渐近残差。
非零亮度升降均平滑；目标为 0 时重置滤波器并立即写 PWM=0，正常在下一个 5kHz PWM 更新点（最长约 200μs）生效。
中途改变目标保持当前滤波状态；不阻塞主循环，不改变心跳周期。
`FILL_LIGHT_FILTER_PERIOD_MS`、`FILL_LIGHT_FILTER_TAU_MS` 在 `service/srv_fill_light.h` 中可调整。

`drv_fill_light_get()` 是当前写入的 PWM 值；`srv_fill_light_get_target()` 是最后接收的目标值。
应用层应通过 `srv_fill_light_set_target()` 请求平滑调光；直接调用驱动 setter 会绕过滤波。
CAN 0x221 仍上报目标值，使主控可立即确认命令已被接收；不能据该字段判断渐变已完成或灯珠实际发光。

## 验证

Debug/Release 构建、带 ASan/UBSan 的协议与平滑调光测试、日志重复初始化/配置/缓冲区测试。
实际灯光观感、示波器 PWM 和实板 CAN 时序仍需上板确认。
