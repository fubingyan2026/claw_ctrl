# Windows：独立 Arm GNU Toolchain 环境配置

本工程使用 **Arm GNU Toolchain 14.2.Rel1**，目标为 `arm-none-eabi`。版本固定是为了复现构建，不表示它是最新版本。
工具链不依赖 STM32CubeIDE。编译通过 CMake + Ninja，调试通过 VS Code Cortex-Debug + SEGGER J-Link。

## 1. 一键配置

在工程根目录打开 PowerShell，执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/setup-env.ps1 -PortableBuildTools
```

脚本兼容 Windows PowerShell 5.1 / PowerShell 7。`-ExecutionPolicy Bypass` 只作用于这一次进程，不修改系统执行策略。
需要 Windows x64、网络连接及足够解压空间（建议至少 2GB）。无需管理员权限，也不写系统或用户 PATH。
下载和解压可能需要几分钟；工具链压缩包约 305MB，请等待脚本显示 `Setup complete`。

脚本执行以下工作：

1. 从 Arm 官方托管地址下载 Windows `arm-none-eabi` ZIP，并校验固定 SHA-256。
2. 将独立 GCC/GDB 安装到 `.tools/arm-gnu-toolchain-14.2.rel1-mingw-w64-i686-arm-none-eabi/`。
3. 使用 `-PortableBuildTools` 时，将 CMake 3.31.6、Ninja 1.12.1 也下载到 `.tools`，无需 Python。省略该参数时优先复用 PATH 已有的 CMake/Ninja，缺少时自动下载。
4. 查找已安装的 J-Link GDB Server。
5. 生成 `.tools/environment.json`，更新 `.vscode/settings.json` 中工具路径，保留其他 VS Code 设置。
6. 项目本地工具齐备后，自动裁剪当前 C/ASM + Cortex-M33 hard-float 之外的冗余组件，并移除下载缓存。用 `-KeepFullTools` 可跳过裁剪。

`.tools` 已加入 `.gitignore`。脚本可重复运行，已有工具会直接复用，不重复下载；已缓存的 ZIP 会先校验再使用。
脚本不自动安装 VS Code、插件或 SEGGER J-Link。当前电脑已具备这些软件。

## 2. 编译

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1 Debug
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1 Release
```

构建脚本自动加载环境。检测到编译器路径改变时，使用 `cmake --fresh` 刷新构建配置，避免继续使用旧的 STM32CubeIDE 编译缓存。
也可手动刷新：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1 Debug -Fresh
```

产物位于 `build/Debug` 或 `build/Release`，包括 ELF、HEX、BIN、MAP。

直接使用命令行工具时，在当前 PowerShell 中执行：

```powershell
# 如当前终端执行策略禁止脚本，可只在当前进程临时放行：
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
. ./scripts/env.ps1
arm-none-eabi-gcc --version
arm-none-eabi-gdb --version
cmake --version
ninja --version
cmake --preset Debug
cmake --build --preset Debug
```

`env.ps1` 必须用开头的点号加载，环境才保留在当前终端中。关闭终端即失效；无需修改全局 PATH。

## 3. VS Code 编译与 J-Link 调试

需要以下插件：

| 插件 | 标识 |
|---|---|
| CMake Tools | ms-vscode.cmake-tools |
| C/C++ | ms-vscode.cpptools |
| Cortex-Debug | marus25.cortex-debug |

本机均已安装。新电脑可在 VS Code 扩展界面的“推荐”中安装，或在 `code` 命令可用时执行：

```powershell
code --install-extension ms-vscode.cmake-tools
code --install-extension ms-vscode.cpptools
code --install-extension marus25.cortex-debug
```

1. 运行配置脚本后，用 VS Code 打开整个工程目录。
2. 若 VS Code 已经打开，执行 `Developer: Reload Window`。
3. 选择 CMake Configure Preset `Debug`。首次切换旧缓存时先运行一次 `scripts/build.ps1 Debug`，或者执行 `CMake: Delete Cache and Reconfigure`。
4. Ctrl+Shift+B 编译；F5 选择 `GD32F503 J-Link (SWD)`，构建后下载并停在 `main`。

J-Link 设备名称为 `GD32F503KEU7`。U3：脚1 GND、脚2 SWDIO、脚3 SWCLK、脚4 VTref/3V3，板子正常供电。详细引脚见 [MCU_PIN.md](../hardware/MCU_PIN.md)。
实板下载、断点和外设工作状态仍需要连接硬件验证。

如果脚本未找到 J-Link，可安装 [SEGGER J-Link 软件](https://www.segger.com/downloads/jlink/)，然后重新运行脚本；或者显式指定：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/setup-env.ps1 -JLinkServerExe "C:/Program Files/SEGGER/JLink_V950/JLinkGDBServerCL.exe"
```

## 4. 已安装工具或离线配置

使用现成的独立工具链（路径需要按实际安装位置修改）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/setup-env.ps1 -ArmGccBin "D:/Tools/arm-gnu-toolchain/bin" -CMakeExe "D:/Tools/cmake/bin/cmake.exe" -NinjaExe "D:/Tools/ninja/ninja.exe" -Offline
```

`-ArmGccBin` 必须指向含 `arm-none-eabi-gcc.exe`、`gdb.exe`、`objcopy.exe`、`objdump.exe`、`size.exe` 的 bin 目录；不接受 STM32CubeIDE 工具链。

从浏览器下载官方 ZIP 后安装：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/setup-env.ps1 -ArchivePath "D:/Downloads/arm-gnu-toolchain-14.2.rel1-mingw-w64-i686-arm-none-eabi.zip"
```

离线安装也可加 `-Offline`；此时 CMake/Ninja 必须已经安装，或将对应 ZIP 放入 `.tools/downloads`：

| 文件名 | 固定版本 |
|---|---|
| arm-gnu-toolchain-14.2.rel1-mingw-w64-i686-arm-none-eabi.zip | Arm GNU 14.2.Rel1 |
| cmake-3.31.6-windows-x86_64.zip | CMake 3.31.6 |
| ninja-win-1.12.1.zip | Ninja 1.12.1，下载时原名为 ninja-win.zip |

移动工程目录后，重新运行配置脚本并用 `-Fresh` 构建，更新本机绝对路径。
自定义环境下，CMake 工具链选择优先级为 `-DARM_GCC_BIN=...`、环境变量 `ARM_GCC_PATH`、`.tools/environment.json`、PATH。
上述 CMake 选择逻辑用于命令行高级配置；常规构建和调试请统一由 setup 脚本生成环境，避免 GCC 和 GDB 路径不一致。

## 本机验证结果

已完成在线 ZIP 校验安装、离线重复配置、Debug/Release 构建，以及仅保留 Windows 系统 PATH 时的 Debug 全量构建。
实际版本为 GCC 14.2.1、GDB 15.2.90、CMake 3.31.6、Ninja 1.12.1。
Debug BIN 15612 字节，Release BIN 13468 字节；编译命令中均使用 `.tools` 的独立编译器。尚未连接实板验证调试和外设。

## 5. 常见问题

- **找不到环境文件**：先运行 `scripts/setup-env.ps1`。
- **SHA-256 不匹配**：下载文件可能不完整；检查网络，并重新下载该 ZIP。脚本不会执行未通过校验的包。
- **仍显示旧编译器**：执行 `scripts/build.ps1 Debug -Fresh` 并重新加载 VS Code；检查 `.tools/environment.json` 和 `.vscode/settings.json`。
- **J-Link GDB Server 找不到或型号不支持**：安装支持 GD32F503KEU7 的 J-Link 软件，重新配置；编译不需要连接调试器。
- **新编译器下程序表现不同**：构建通过只说明源码和工具链兼容，更换版本后的固件仍需做实板回归验证。

## CMake 面板显示“未知 / 项目配置问题”

本机 STM32Cube Build Analyzer 1.5.0 显示在 CMake 侧栏，但通过 STM32Cube 项目接口寻找分析目标。本工程是 GD32 普通 CMake 工程，因此它无法获取有效目标，显示“未知”不代表 GNU 编译失败。
本机已仅在当前工作区禁用 `STMicroelectronics.stm32cube-ide-build-analyzer`；其他 STM32 工程仍可使用它。新电脑出现同样提示时，在扩展列表找到 STM32Cube Build Analyzer，选择“禁用(工作区)”后重新加载窗口。保留 Microsoft CMake Tools、C/C++ 和 Cortex-Debug。

查看本工程内存占用：命令面板 → `Tasks: Run Task`（任务: 运行任务）→ `Memory usage (Debug)`；或执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1 Debug -Target memory
```

清理并重新编译：运行 `Clean rebuild Debug` 任务，或：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1 Debug -Clean
```

CMake 侧栏也会提供 `memory` 目标。内存报告使用当前独立工具链读取 ELF，Flash 为 text + data，RAM 为 data + BSS（包含链接脚本预留堆栈），无需 STM32Cube 分析器。

## 官方来源及校验值

- [Arm GNU 安装说明](https://learn.arm.com/install-guides/gcc/arm-gnu/)
- [固定版 Arm GNU ZIP](https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-mingw-w64-i686-arm-none-eabi.zip)
- [Arm 官方 SHA-256](https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-mingw-w64-i686-arm-none-eabi.zip.sha256asc)
- [CMake 3.31.6 发布与校验值](https://github.com/Kitware/CMake/releases/tag/v3.31.6)
- [Ninja 1.12.1 发布](https://github.com/ninja-build/ninja/releases/tag/v1.12.1)

Arm GNU ZIP SHA-256：`6facb152ce431ba9a4517e939ea46f057380f8f1e56b62e8712b3f3b87d994e1`。
CMake ZIP SHA-256：`d163cd3ab4959b0a53fa8988f2ddbd2e6c501658201e6a154386bad9dbe4f836`。
Ninja ZIP SHA-256：`f550fec705b6d6ff58f2db3c374c2277a37691678d6aba463adcbb129108467a`（固定为本次从官方发布资产下载验证的内容）。
