# claw_ctrl

GD32F503KEU7 夹爪控制板固件，参考 `C:/Users/32457/Desktop/cubemx_project/STM32C092FBPx` 的功能与分层架构。
硬件连接以 `hardware/SCH_T0_HW1.0_夹爪控制板-261025.pdf` 第2页（2026-11-04）为准；引脚详情见 [MCU_PIN.md](hardware/MCU_PIN.md)。

## 换电脑一键编译

Windows x64 上双击根目录 `build.cmd` 编译 Debug，双击 `build-release.cmd` 编译 Release。
工具缺失时首次自动下载并校验，之后复用工程内工具；无需管理员权限、手动 PATH、Python 或 VS Code。

**离线交付推荐**：在本机双击 `package-portable.cmd`，把 `dist` 中生成的 ZIP 复制到其他 Windows x64 电脑，解压后双击 `build.cmd`。
离线精简包约 58 MiB，包含源码和本工程所需编译工具，不包含旧构建缓存，支持工程位置变化。不要直接在 ZIP 预览中运行脚本。
详见 [一键编译与离线交付](docs/portable-build.md)。编译无需 J-Link；烧录调试仍需要安装 J-Link 软件与驱动。

## 功能与目录

- `Core/Inc`、`Core/Src`：板级初始化、SysTick 1ms、异常入口和主循环。
- `Drivers`：GD32F50x V1.0.4 官方 CMSIS 和标准外设库的本地副本；构建只引用该目录，不引用 STM32 HAL。
- `device_drivers`：CAN0 中断接收及 RX/TX 队列、USART0 TX DMA、RGB PWM、补光灯 PWM。
- `m_middlewares`：沿用日志、FIFO、mpaland printf。
- `service`：沿用巨蟹双电机状态解析与 RGB 灯效映射。
- `tasks`：沿用裸机轮询任务，时基接口改为 `board_millis()`。
- `cmake`、`.vscode`、`scripts`：GNU Arm 工具链、构建预设、VS Code 编译调试配置。
- `tests`：业务逻辑与消息队列的主机回归测试。

双电机 Dev_ID 默认 1/2；反馈 ID 为 0x301/0x302，支持参考工程的 12/16 字节反馈。
CAN FD 使用 ISO 模式，仲裁段/数据段均 1Mbps，不启用 BRS。
空闲 500 ms 后周期发送 0x80 同步查询。电机反馈超时为 1000 ms，0x701 升级会话超时为 6000 ms。
状态指示：两台电机均正常使能时白灯常亮。任一台故障或只检测到一台时红灯常亮。未就绪或未全部使能时绿灯闪烁，升级时蓝灯闪烁。闪烁状态完成当前周期（亮 500 ms、灭 500 ms）后再切换。电机状态中故障优先。
夹爪板自身通过 CAN 在 App 中接收升级，蓝灯闪烁，重启后由 Boot 安装新固件。详见 [CAN 升级与烧录](docs/firmware-upgrade.md)。

补光灯通过 `0x220` 设置 5 kHz PWM 亮度（0..1000 对应 0..100% 占空比，0 立即关灯，非零平滑变化），上电默认关闭；控制板每 500ms 主动发送 `0x221` 心跳/状态，合法调光命令另触发即时状态上报。完整字段、示例和时序见 [CAN 协议 V1.2](docs/can-protocol.md)。
USART0 为 PB6 TX / PB7 RX，115200 8N1。日志 TX 使用 DMA0 CH3；RX 引脚已配置，未实现 RX DMA/接收协议。

## 编译

本机已配置并验证：

- 使用独立 Arm GNU Toolchain 14.2.Rel1，编译目标为 Cortex-M33 + FPv5-SP，hard float。
- 使用 `scripts/setup-env.ps1` 配置 CMake、Ninja、Arm GCC/GDB 和 VS Code；工具路径保存在 `.tools/environment.json`。
- Flash 512KB、SRAM 64KB。Boot、App A、App B 和升级参数使用固定分区，每个可执行镜像保留 2KB 栈和 1KB 堆。
- 官方 8MHz 外部晶振配置提供 252MHz 系统时钟；CAN 时序及 PWM 分频与此配置配套。

首次配置（无管理员权限，详见 [Windows 环境教程](docs/windows-toolchain.md)）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/setup-env.ps1 -PortableBuildTools
```

在项目根目录编译：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1 Debug
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1 Release
```

激活当前终端环境后，也可直接执行：

```powershell
. ./scripts/env.ps1
cmake --preset Debug
cmake --build --preset Debug
```

输出位于 `build/Debug` 或 `build/Release`。`claw_factory.hex` 是烧录器整包，`claw_factory.bin` 是同一整包的 BIN 格式，`claw_ctrl.bin` 用于 CAN 升级。Boot 和 App 各自的 ELF、HEX、BIN 和 MAP 也会生成。
移到其他电脑或更换工程位置后，重新运行 `scripts/setup-env.ps1` 生成本机配置。
工程不再查找 STM32CubeIDE；可用 `-ArmGccBin` 指定其他独立工具链的 bin 目录，切换编译器时构建脚本自动刷新 CMake 缓存。

## VS Code + J-Link

以下插件本机均已安装，无需重复安装：

| 插件 | ID | 用途 |
|---|---|---|
| CMake Tools | ms-vscode.cmake-tools | 选择预设、配置与构建 |
| C/C++ | ms-vscode.cpptools | 补全、代码导航，配置由 CMake 提供 |
| Cortex-Debug | marus25.cortex-debug | J-Link SWD 调试 |

1. 先运行环境脚本，再用 VS Code 打开本工程根目录；若已打开，运行 `Developer: Reload Window`，选择 CMake Configure Preset `Debug`。
2. 按 Ctrl+Shift+B 运行默认 `Build Debug`；也可通过 CMake Tools 构建。
3. 按原理图连接 J-Link：U3 脚1 GND、脚2 SWDIO、脚3 SWCLK、脚4 VTref(3V3)。板子正常供电。U3 没有 NRST。
4. 在“运行和调试”选择 `GD32F503 J-Link (SWD)`，按 F5。构建后下载 `claw_factory.hex`，用 App ELF 提供调试符号并停在 App 的 `main`。

已检查本机 J-Link V9.50 的设备库包含精确型号 `GD32F503KEU7`，launch.json 使用此名称。
尚未连接实板验证下载、断点、CAN 通信、PWM 波形和日志输出。

## 验证与上板检查

主机回归测试（依赖现有 WSL Ubuntu 和 GCC；固件编译无需 WSL）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/test.ps1
```

测试启用 AddressSanitizer 和 UndefinedBehaviorSanitizer，覆盖双电机正常/故障/恢复/离线、升级会话超时、空闲查询、灯效、消息队列满/空与 32位时基/队列计数回绕。
Debug、Release 构建均已通过，未出现编译和链接警告。主机测试不替代外设和实板验证。
上板需确认：115200 日志、红绿蓝顺序、PA6 上电为低、补光灯 5kHz PWM（RGB 仍为 1kHz）、CAN FD 1Mbps 无 BRS 下的反馈/同步帧，以及 Bus-Off 后恢复。

## 移植说明与来源

原 STM32 HAL 外设初始化及中断实现已替换为 GD32 标准库调用；服务层保留参考工程行为。
原 `MCU_PIN.md` 中的 RGB 引脚和 UART 方向已按图纸修正。RGB 颜色按 LED3 型号的数据手册核对：PA1 红、PA3 绿、PA2 蓝。
`FDCAN1_*` 和 `LED_TIM3_*` 仅保留为原理图网络名，实际使用 CAN0、TIMER1、TIMER2。
官方库文件保持原样；根目录链接脚本将初始化数组标记为只读，以消除 GNU ld 的 RWX 段警告。

- [GD32F50x 官方库下载页](https://www.gd32mcu.com/en/download?kw=GD32F5)
- [GD32F503 官方数据手册](https://www.gd32mcu.com/download/down/document_id/743/path_type/1)
- [LED3 Kingbright 数据手册](https://www.kingbrightusa.com/images/catalog/SPEC/AAA3528LSEEZGKQBKS.pdf)
- [Cortex-Debug 配置说明](https://github.com/Marus/cortex-debug/blob/master/debug_attributes.md)

官方库及参考代码保留各文件原版权说明，许可说明以 `Drivers` 中各文件保留的原始声明及所获取官方 SDK 的许可文件为准。

中间件已从 R1_CTU_POWER 导入，详见 [移植说明](m_middlewares/PORTING.md)。补光灯非零亮度采用 PT1 平滑，约 100ms 时间常数，关灯立即执行；心跳回报目标亮度。
