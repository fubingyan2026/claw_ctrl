# MCU 引脚分配（以原理图为准）

主控 U4：**GD32F503KEU7，QFN32，Cortex-M33，512 KB Flash / 64 KB SRAM**。
核对依据：`SCH_T0_HW1.0_夹爪控制板-261025.pdf` 第 2 页 MCU，图纸更新日期 2026-11-04；GD32F503 数据手册 Rev0.9RC6 表 2-11/2-12。

原理图的 `LED_TIM3_CHx`、`FDCAN1_*` 是网络标号，不表示 GD32 的实际外设编号。
本表保留网络原名，单独给出固件采用的复用。

| 脚号 | MCU 引脚 | 原理图网络 | 实际用途 / 固件配置 |
|---|---|---|---|
| 1 | VDD | 3V3 | 数字电源 |
| 2 | OSCIN-PD0 | OSC_IN | 8 MHz 晶振 X1 输入 |
| 3 | OSCOUT-PD1 | OSC_OUT | 8 MHz 晶振 X1 输出 |
| 4 | NRST | NRST | R22 上拉、C13 滤波；TP13 |
| 5 | VDDA/VREFP | 3V3 | 模拟电源及参考电压 |
| 6 | PA0 | NC | 原理图未连接 |
| 7 | PA1 | LED_TIM3_CH3 | AF1，TIMER1_CH1，RGB 红；R6 → LED3 脚1 |
| 8 | PA2 | LED_TIM3_CH2 | AF1，TIMER1_CH2，RGB 蓝；R8 → LED3 脚4 |
| 9 | PA3 | LED_TIM3_CH4 | AF1，TIMER1_CH3，RGB 绿；R7 → LED3 脚3 |
| 10 | PA4 | NC | 原理图未连接 |
| 11 | PA5 | NC | 原理图未连接 |
| 12 | PA6 | FILL_LIGHT_TIM3_CH1 | AF1，TIMER2_CH0；接 U8 TX6128 的 DIM 脚3 |
| 13 | PA7 | NC | 原理图未连接 |
| 14 | PB0 | NC | 原理图未连接 |
| 15 | PB1 | NC | 原理图未连接 |
| 16 | PB2/BOOT1 | NC | 原理图未连接 |
| 17 | VDD | 3V3 | 数字电源 |
| 18 | PA8 | NC | 原理图未连接 |
| 19 | PA9 | NC | 原理图未连接 |
| 20 | PA10 | FDCAN1_STD | GPIO 推挽；经 R11 接 TCAN1044V STB，低为正常，高为待机 |
| 21 | PA11 | FDCAN1_RX | AF1，CAN0_RX，经 R14 接收发器 RXD；TP15 |
| 22 | PA12 | FDCAN1_TX | AF1，CAN0_TX，经 R13 接收发器 TXD；TP14 |
| 23 | PA13 | SWDIO | SWD 数据；U3 脚2，TP10 |
| 24 | VDD | 3V3 | 数字电源 |
| 25 | PA14 | SWCLK | SWD 时钟；U3 脚3，TP7 |
| 26 | PA15 | NC | 原理图未连接 |
| 27 | PB3 | NC | 原理图未连接 |
| 28 | PB4 | NC | 原理图未连接 |
| 29 | PB5 | NC | 原理图未连接 |
| 30 | PB6 | UART_TX | AF0，USART0_TX；CN1 脚2，TP3 |
| 31 | PB7 | UART_RX | AF0，USART0_RX；CN1 脚3，TP4 |
| 32 | BOOT0 | BOOT0 | 经 R17 10 kΩ 下拉至 GND |
| EPAD | VSS/VSSA | GND | 中心裸露焊盘接地 |

## 接口与时序

- U3 SWD：脚1 GND、脚2 SWDIO、脚3 SWCLK、脚4 3V3；NRST 未接此连接器。3V3 用作调试器参考电压，板子由电源接口供电。
- CN1 UART：脚1 GND、脚2 MCU TX、脚3 MCU RX；连接 USB-UART 时交叉接收发，3.3 V 电平，115200 8N1。
- RGB LED3 为共阳：脚2 接 3V3，脚1红、脚3绿、脚4蓝。PWM 1 kHz，亮度 0..1000，CCR=1000-level，初始化熄灭。
- 补光灯 PA6 为高有效 PWM，5 kHz（周期 200 μs），直连 U8 DIM，使用数字 PWM 调光；默认占空比 0；`drv_fill_light_set()` 设置 0..1000（0..100% 占空比）；CAN 0 立即关灯，非零有效值平滑调光，不使用 1.1..3.1 V 模拟调光表；CAN `0x220` 用于设置和查询，`0x221` 回复状态；详见 docs/can-protocol.md。
- CAN0 使用 ISO CAN FD，仲裁/数据均 1 Mbps，无 BRS；自动 Bus-Off 恢复，关闭自动重发，与参考工程发送策略一致。
- 使用官方系统时钟配置：外部 8 MHz → PLL → 252 MHz；AHB/APB2 252 MHz、APB1 126 MHz，TIMER1/2 时钟 252 MHz。

## 更正记录

旧表将 PA0/PA1/PA7 写作 RGB、将 PB6/PB7 收发方向写反，并混用了 STM32 的定时器编号及部分测试点编号。本版按原理图连线修正。
RGB 颜色依据 [Kingbright AAA3528LSEEZGKQBKS 数据手册](https://www.kingbrightusa.com/images/catalog/SPEC/AAA3528LSEEZGKQBKS.pdf) 引脚定义。
芯片复用依据 [GD32F503 官方数据手册](https://www.gd32mcu.com/download/down/document_id/743/path_type/1)。
