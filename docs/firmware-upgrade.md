# CAN 升级与烧录

## 首次烧录

运行 `build-release.cmd`，在 `build/Release` 取得以下文件。Debug 构建生成同名文件。

| 文件 | 用途 |
| --- | --- |
| `claw_factory.hex` | 烧录器整包，含 Boot、App A、初始参数 |
| `claw_factory.bin` | 同一整包的 BIN 格式，烧录起始地址为 `0x08000000` |
| `claw_factory.jlink` | J-Link Commander 整包擦除、写入、校验并复位脚本 |
| `claw_ctrl.bin` | 上位机 CAN 升级用 App 固件 |

从旧版固件迁移时先用烧录器写入整包。手动使用烧录器写入整包前执行全片擦除，清除旧镜像和 ring_storage 记录。也可从构建输出目录运行生成的脚本，脚本包含全片擦除。

```powershell
cd build/Release
& "C:/Program Files/SEGGER/JLink_V950/JLink.exe" -Device GD32F503KEU7 -If SWD -Speed 4000 -AutoConnect 1 -CommandFile claw_factory.jlink
```

## 在线升级

在 canable 的 `boot_upgrade` 插件中选择 `claw_ctrl.bin`，设置 Host CAN ID 为 `0x703`，插件自动接收 `0x704`。硬件 ID 为 `0x0001`。使用标准帧，CAN FD 仲裁段和数据段均为 1 Mbps，BRS 关闭；帧长度推荐 64 字节，经典 CAN 可选 8 字节。

插件的「帧间隔」设置相邻升级发送帧的最小间隔，默认 1 ms，范围 0–50 ms。0 ms 表示连续发送时无额外延时。升级日志显示同一块内相邻数据帧的平均和最短主机发送间隔。新版夹爪固件内置版本，来自 `CMakeLists.txt` 的 `project(... VERSION ...)`。插件在开始升级前显示文件内置版本，旧格式文件继续使用「传输版本」。

连接后点击「读取固件信息」，可查看当前 App 版本、启动次数、元数据修订、固件大小、CRC32 和上次安装结果。启动次数从新版 Boot 第一次启动开始累计，包含上电和升级安装时进入 Boot。查询只读，修订号是 ring_storage 提交计数。已有旧 Boot 的板子需更新整包以启用计数和安装结果记录。

App 按 START、METADATA、逐个 1 KiB 块、VERIFY、REBOOT 的顺序接收。START 和 METADATA 的字段按 STM32G474 固件及 canable 插件实际使用的字节偏移解析。每块校验 16 位累加和并进行 Flash 读回比对。VERIFY 核对整包 32 位累加和、目标标识及 A 槽向量。REBOOT 提交待安装记录并应答，Boot 复位后再次验证并安装。

接收与校验期间蓝灯闪烁。收到 REBOOT 后先应答，当前蓝灯周期（亮 500 ms、灭 500 ms）结束后进入 Boot。取消或会话超时会结束接收，指示灯在当前周期结束后恢复电机状态。Boot 安装时蓝灯常亮，App 启动后恢复电机状态指示。Boot 无法校验或安装可启动镜像时显示红灯，使用烧录器恢复。

CAN 下载期间，补光灯调光和电机同步查询继续运行。使用新版 Boot 和 App 时，App 应答后直接跳入 Boot 安装固件，保留 TIMER2 和系统时钟。新 App 接管正在输出的 PWM，并接续原亮度和目标。CAN 和串口在新 App 启动后恢复。

已安装旧 Boot 的板子需要用新版整包更新一次。仅升级 App 时，旧 Boot 仍通过系统复位进入安装流程，复位会中断 PWM。复位行为见 [GD32F50x 用户手册 §9.1](https://www.gd32mcu.com/data/documents/userManual/GD32F50x_User_Manual_Rev1.4.pdf)。

## Flash 布局

| 区域 | 起始地址 | 容量 |
| --- | --- | --- |
| Boot | `0x08000000` | 32 KiB |
| App A | `0x08008000` | 236 KiB |
| App B | `0x08043000` | 236 KiB |
| ring_storage 参数 | `0x0807E000` | 8 KiB |

App 固定链接在 A 槽。CAN 下载期间只擦写 B 槽，Boot 在待安装记录提交后才擦写 A 槽。复制过程中断电时，Boot 下次启动会用 B 槽重新安装。整包 CRC32 校验覆盖固件实际长度，ring_storage 按 2 KiB 页轮转保存当前和待安装镜像参数。Flash 页大小与字编程见 [GD32F50x 用户手册 §6](https://www.gd32mcu.com/data/documents/userManual/GD32F50x_User_Manual_Rev1.4.pdf)。

## 固件信息帧

- 请求 ID `0x703`，8 字节 `[09][查询号][00 × 6]`。
- 响应 ID `0x704`，共 5 个 8 字节经典 CAN 帧。每帧为 `[12][查询号][页号 0–4][5 字节数据]`。
- 按页号拼接后的 25 字节均为大端序，依次为 App 版本、启动次数、元数据修订、有效固件大小、CRC32（各 4 字节）、硬件 ID（2 字节）、标志、安装结果、安装错误（各 1 字节）。
- 版本编码为 `major << 16 | minor << 8 | patch`。标志 bit0 表示元数据有效，bit1 表示有待安装镜像。安装结果 0/1/2 为无记录/成功/失败，安装错误 0/1/2 为无错误/镜像无效/Flash 写入或校验失败。

镜像头在 App 偏移 `0x200`。原有签名和目标标识之后追加版本标识 `0x56524531` 与版本号。ring_storage 保留原 `image` 键及 24 字节布局，追加 `stats` 键保存启动次数和安装结果。已有元数据缺少 `stats` 时从零开始。
