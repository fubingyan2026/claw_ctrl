# 一键编译与离线交付（Windows x64）

目标电脑无需事先安装 GCC、CMake、Ninja、Python、VS Code，也无需配置 PATH 或管理员权限。需要 Windows 自带的 Windows PowerShell 5.1。
工具仍然存在，只是随工程携带或由脚本首次自动下载。支持 Windows x64；不包含 macOS/Linux 的编译工具。

## 日常使用

- 双击 `build.cmd`：Debug。
- 双击 `build-release.cmd`：Release。
- 成功后产物在 `build/Debug` 或 `build/Release`。烧录器用 `claw_factory.hex`，CAN 升级用 `claw_ctrl.bin`。详见 [CAN 升级与烧录](firmware-upgrade.md)。
- 窗口会显示编译结果并等待按键，报错时可直接查看原因。

工具缺失时首次运行会从官方地址下载固定版本并验证 SHA-256；有工具时直接复用 `.tools`。
每次运行自动更新 `.tools/environment.json` 和 VS Code 路径。若复制时带上旧 CMakeCache，脚本也会检查工程位置并刷新配置。
无需事先运行 setup-env.ps1。工程应放在可写目录，中文和空格路径已验证；避免路径过长，建议解压到较短的工程路径。

## 方案一：联网电脑只拿源码

复制源码（含 Drivers、脚本与根目录 .cmd 文件），或者从仓库获取工程，然后双击 `build.cmd`。
首次需要网络，自动下载独立 Arm GNU 14.2.Rel1、CMake 3.31.6 和 Ninja 1.12.1。下载量约几百 MB，工具解压建议预留至少 2GB 空间。
后续编译无需网络。

## 方案二：制作完整离线包（推荐给同事交付）

在已准备好工具的电脑双击 `package-portable.cmd`。
脚本在 `dist` 中生成带时间戳的 `claw_ctrl-portable-slim-win-x64-*.zip`。
将该 ZIP 复制到目标 Windows x64 电脑，**完整解压**，再双击 `build.cmd` 或 `build-release.cmd`。

打包内容包括：

- 工程源码、GD32 标准库、硬件资料、配置与使用说明。
- `build.cmd`、`build-release.cmd` 及 PowerShell 脚本。
- `.tools` 中固定版本的 Arm GNU、CMake、Ninja 的项目专用精简运行环境（含原始许可证）。

排除 `.git`、旧 `build`、`tmp`、`dist`、下载缓存和本机 `.tools/environment.json`。
首次编译会重新生成本机路径，不需要两台电脑使用相同用户名、盘符或工程目录。
ZIP 中的隐藏目录 `.tools`、`.vscode` 必须完整保留；不要只复制可见源文件。
目标机缺少工程编译工具时，首次运行会尝试联网下载。

## 仅烧录离线包

运行 `package-portable.cmd -FlashOnly -Offline` 可生成 `dist/claw_ctrl-factory-flash-win-x64-*.zip`。其中包含 Release 整包 BIN、J-Link 命令文件和双击烧录脚本。目标机完整解压后可直接烧录预编译的固件。

目标机从 SEGGER 获取自己的 J-Link Commander。烧录脚本依次查找项目内 `.tools/jlink/JLink.exe`、PATH、Program Files 和当前用户的 SEGGER 安装目录，也支持用 `-JLinkExe` 指定路径。SEGGER 的许可要求分发 Commander 前取得书面授权，生成的 ZIP 由用户在目标机配合官方安装使用。[SEGGER 使用条款](https://www.segger.com/products/debug-probes/j-link/tools/terms-of-use/) · [官方安装包](https://www.segger.com/downloads/jlink/)

## 命令行选项

在工程根目录 CMD 或 PowerShell 中：

```powershell
.uild.cmd Debug -Offline
.uild-release.cmd -Offline
.uild.cmd Debug -Clean
.\package-portable.cmd -Offline
```

`-Offline` 禁止工具下载，缺少工具或缓存时会清晰报错；携带完整工具的离线包可直接使用。
CI/自动化调用可设置 `CLAW_BUILD_NO_PAUSE=1` 跳过结束等待；脚本成功返回 0，失败返回非 0。

```powershell
$env:CLAW_BUILD_NO_PAUSE='1'
.uild.cmd Release -Offline
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
```

## 编译与烧录调试

一键编译无需连接开发板，也无需 J-Link 软件。
烧录需要 SEGGER J-Link Commander。支持 WinUSB 的探针可使用 Windows 内置驱动，部分旧探针需要 SEGGER USB 驱动。通过 VS Code 调试还需要 J-Link GDB Server、VS Code、CMake Tools、C/C++ 和 Cortex-Debug。[SEGGER WinUSB 说明](https://kb.segger.com/J-Link_WinUSB_driver_selection)
运行一键编译会自动查找该电脑的 J-Link GDB Server，更新 VS Code 路径；找不到时只提示警告，不阻止编译。

## 验证范围

已验证使用工程内固定工具、仅保留 Windows 系统 PATH 的编译；另在带中文与空格的新工程路径中，携带旧 CMakeCache 后执行离线 Debug 全量构建，验证路径更新和缓存迁移。
目标电脑若通过企业策略禁止脚本执行，仍需管理员按组织策略处理；本方案不修改系统执行策略。

## 精简环境范围

默认打包和本地自动准备流程仅保留当前工程需要的 **C / 汇编、Cortex-M33、FPv5 单精度、hard-float** 环境。
原离线 ZIP 约 405.4 MiB，精简 ZIP 约 58 MiB；工具自身解压后约 154 MiB（连同工程约 157 MiB）。

已移除其他 Arm 架构及浮点 ABI 的运行库、C++ 和 Fortran 编译器/库、离线手册/示例、CMake GUI、CPack 和覆盖率分析工具。
保留 GCC、汇编/链接工具、对应 C/newlib/nano 运行库、LTO 后端、GDB、符号查看工具、CMake/CTest、Ninja 及版权许可。
打包不修改源码和固件算法；已验证精简环境离线 Debug/Release 全量构建、内存报告和 GDB 调试符号读取。

首次联网安装仍下载官方完整 SDK 以校验来源，安装后自动裁剪并移除下载缓存；分发精简离线包可以避免目标机下载完整 SDK。
脚本不修改由 `-ArmGccBin` 指定的外部工具链。本机旧版本重复解压副本可使用以下显式清理命令：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/trim-tools.ps1 -RemoveDownloadCache -RemoveLegacyFlatCopy
```

如需 C++、其他 MCU/ABI，需重新解压官方完整工具链，并使用 `setup-env.ps1 -PortableBuildTools -KeepFullTools` 保留完整环境。
`package-portable.cmd -FullTools` 可打包已恢复的完整 SDK；`-KeepFullTools` 本身不会恢复已经裁剪的文件。
精简运行库选择由 GCC `-print-multi-directory` 和 `-print-file-name` 查询核对，见 [GCC 官方选项说明](https://gcc.gnu.org/onlinedocs/gcc/Developer-Options.html)。
