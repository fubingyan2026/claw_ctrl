[CmdletBinding()]
param(
    [string]$ArmGccBin,
    [string]$CMakeExe,
    [string]$NinjaExe,
    [string]$JLinkServerExe,
    [string]$ArchivePath,
    [switch]$PortableBuildTools,
    [switch]$KeepFullTools,
    [switch]$Offline
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
$projectRoot = Split-Path $PSScriptRoot -Parent
$toolsRoot = Join-Path $projectRoot '.tools'
$downloads = Join-Path $toolsRoot 'downloads'
New-Item -ItemType Directory -Force $downloads | Out-Null

function Get-VerifiedZip {
    param([string]$Url, [string]$Destination, [string]$Sha256)
    if (Test-Path -LiteralPath $Destination) {
        if ((Get-FileHash -LiteralPath $Destination -Algorithm SHA256).Hash -eq $Sha256) { return }
        throw "Checksum mismatch: $Destination. Supply a complete official archive or download it again."
    }
    if ($Offline) { throw "Offline mode: archive not found: $Destination" }
    Write-Host "Downloading $Url"
    $partial = "$Destination.part"
    Invoke-WebRequest -UseBasicParsing -Uri $Url -OutFile $partial -TimeoutSec 1800
    if ((Get-FileHash -LiteralPath $partial -Algorithm SHA256).Hash -ne $Sha256) {
        throw "SHA256 verification failed: $partial"
    }
    Move-Item -LiteralPath $partial -Destination $Destination -Force
}
function Expand-ToolZip {
    param([string]$Archive, [string]$Destination)
    New-Item -ItemType Directory -Force $Destination | Out-Null
    $tar = Get-Command tar.exe -ErrorAction SilentlyContinue
    if ($tar) {
        & $tar.Source -xf $Archive -C $Destination
        if ($LASTEXITCODE -ne 0) { throw "Archive extraction failed: $Archive" }
    } else {
        Expand-Archive -LiteralPath $Archive -DestinationPath $Destination -Force
    }
}
function Find-Executable {
    param([string]$Name)
    $command = Get-Command $Name -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($command) { return $command.Source }
    return $null
}
function Resolve-Executable {
    param([string]$File)
    if (!$File -or !(Test-Path -LiteralPath $File -PathType Leaf)) { throw "Executable not found: $File" }
    return (Resolve-Path -LiteralPath $File).Path
}

$armPackage = 'arm-gnu-toolchain-14.2.rel1-mingw-w64-i686-arm-none-eabi'
$armHash = '6facb152ce431ba9a4517e939ea46f057380f8f1e56b62e8712b3f3b87d994e1'
if (!$ArmGccBin) {
    $ArmGccBin = Join-Path $toolsRoot "$armPackage/bin"
    if (!(Test-Path -LiteralPath (Join-Path $ArmGccBin 'arm-none-eabi-gcc.exe')) -or
        !(Test-Path -LiteralPath (Join-Path $ArmGccBin 'arm-none-eabi-gdb.exe'))) {
        if (!$ArchivePath) { $ArchivePath = Join-Path $downloads "$armPackage.zip" }
        $armUrl = "https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu/14.2.rel1/binrel/$armPackage.zip"
        Get-VerifiedZip $armUrl $ArchivePath $armHash
        Write-Host 'Extracting standalone Arm GNU Toolchain...'
        Expand-ToolZip $ArchivePath (Join-Path $toolsRoot $armPackage)
    }
}
$ArmGccBin = (Resolve-Path -LiteralPath $ArmGccBin).Path
if ($ArmGccBin -match '(?i)STM32CubeIDE|gnu-tools-for-stm32') {
    throw 'Select a standalone Arm GNU Toolchain, not an STM32CubeIDE toolchain.'
}
foreach ($name in @('gcc','gdb','objcopy','objdump','size')) {
    $null = Resolve-Executable (Join-Path $ArmGccBin "arm-none-eabi-$name.exe")
}

if (!$CMakeExe -and (Test-Path -LiteralPath (Join-Path $toolsRoot 'cmake-3.31.6-windows-x86_64/bin/cmake.exe'))) {
    $CMakeExe = Join-Path $toolsRoot 'cmake-3.31.6-windows-x86_64/bin/cmake.exe'
}
if (!$CMakeExe -and !$PortableBuildTools) { $CMakeExe = Find-Executable 'cmake.exe' }
if (!$CMakeExe) {
    $cmakePackage = 'cmake-3.31.6-windows-x86_64'
    $CMakeExe = Join-Path $toolsRoot "$cmakePackage/bin/cmake.exe"
    if (!(Test-Path -LiteralPath $CMakeExe)) {
        $zip = Join-Path $downloads "$cmakePackage.zip"
        Get-VerifiedZip "https://github.com/Kitware/CMake/releases/download/v3.31.6/$cmakePackage.zip" $zip 'd163cd3ab4959b0a53fa8988f2ddbd2e6c501658201e6a154386bad9dbe4f836'
        Expand-ToolZip $zip $toolsRoot
    }
}
if (!$NinjaExe -and (Test-Path -LiteralPath (Join-Path $toolsRoot 'ninja-1.12.1/ninja.exe'))) {
    $NinjaExe = Join-Path $toolsRoot 'ninja-1.12.1/ninja.exe'
}
if (!$NinjaExe -and !$PortableBuildTools) { $NinjaExe = Find-Executable 'ninja.exe' }
if (!$NinjaExe) {
    $NinjaExe = Join-Path $toolsRoot 'ninja-1.12.1/ninja.exe'
    if (!(Test-Path -LiteralPath $NinjaExe)) {
        $zip = Join-Path $downloads 'ninja-win-1.12.1.zip'
        Get-VerifiedZip 'https://github.com/ninja-build/ninja/releases/download/v1.12.1/ninja-win.zip' $zip 'f550fec705b6d6ff58f2db3c374c2277a37691678d6aba463adcbb129108467a'
        Expand-ToolZip $zip (Split-Path $NinjaExe -Parent)
    }
}
$CMakeExe = Resolve-Executable $CMakeExe
$NinjaExe = Resolve-Executable $NinjaExe
$cmakeVersion = & $CMakeExe --version
if ($LASTEXITCODE -ne 0) { throw 'CMake cannot run' }
if (($cmakeVersion -join ' ') -notmatch 'cmake version (\d+\.\d+\.\d+)') { throw 'Cannot read CMake version' }
if ([version]$Matches[1] -lt [version]'3.24.0') { throw 'CMake >= 3.24 is required. Supply -CMakeExe with a newer version.' }
& $NinjaExe --version
if ($LASTEXITCODE -ne 0) { throw 'Ninja cannot run' }
& (Join-Path $ArmGccBin 'arm-none-eabi-gcc.exe') --version
if ($LASTEXITCODE -ne 0) { throw 'Arm GCC cannot run' }
& (Join-Path $ArmGccBin 'arm-none-eabi-gdb.exe') --version
if ($LASTEXITCODE -ne 0) { throw 'Arm GDB cannot run' }
if (!$JLinkServerExe) { $JLinkServerExe = Find-Executable 'JLinkGDBServerCL.exe' }
if (!$JLinkServerExe) {
    $candidates = @()
    foreach ($base in @($env:ProgramFiles, ${env:ProgramFiles(x86)})) {
        if ($base) {
            $candidates += Get-ChildItem -Path (Join-Path $base 'SEGGER/JLink*/JLinkGDBServerCL.exe') -ErrorAction SilentlyContinue
        }
    }
    $candidate = $candidates | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if ($candidate) { $JLinkServerExe = $candidate.FullName }
}
if ($JLinkServerExe) { $JLinkServerExe = Resolve-Executable $JLinkServerExe }
else { Write-Warning 'J-Link GDB Server not found. Building works; install SEGGER J-Link and rerun setup before F5.' }

$config = [ordered]@{
    ArmGccBin = $ArmGccBin
    CMakeExe = $CMakeExe
    NinjaExe = $NinjaExe
    JLinkServerExe = $JLinkServerExe
}
$config | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $toolsRoot 'environment.json') -Encoding UTF8
$vscodeRoot = Join-Path $projectRoot '.vscode'
New-Item -ItemType Directory -Force $vscodeRoot | Out-Null
$settingsPath = Join-Path $vscodeRoot 'settings.json'
$settings = if (Test-Path -LiteralPath $settingsPath) { Get-Content -LiteralPath $settingsPath -Raw | ConvertFrom-Json } else { [pscustomobject]@{} }
$toolDirs = @($ArmGccBin, (Split-Path $CMakeExe -Parent), (Split-Path $NinjaExe -Parent)) | Select-Object -Unique
$configureEnv = [pscustomobject]@{ ARM_GCC_PATH = $ArmGccBin; PATH = (($toolDirs -join ';') + ';${env:PATH}') }
$ownedSettings = @{
    'cmake.cmakePath' = $CMakeExe
    'cmake.useCMakePresets' = 'always'
    'cmake.configureOnOpen' = $true
    'cmake.environment' = $configureEnv
    'C_Cpp.default.configurationProvider' = 'ms-vscode.cmake-tools'
    'stm32cube-ide-build-cmake.ignoreCubeProjectDiscovery' = $true
    'cortex-debug.armToolchainPath' = $ArmGccBin
}
if ($JLinkServerExe) { $ownedSettings['cortex-debug.JLinkGDBServerPath'] = $JLinkServerExe }
elseif ($settings.PSObject.Properties['cortex-debug.JLinkGDBServerPath']) {
    $settings.PSObject.Properties.Remove('cortex-debug.JLinkGDBServerPath')
}
foreach ($key in $ownedSettings.Keys) { $settings | Add-Member -MemberType NoteProperty -Name $key -Value $ownedSettings[$key] -Force }
$settings | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $settingsPath -Encoding UTF8
if (!$KeepFullTools -and
    (Test-Path -LiteralPath (Join-Path $toolsRoot "$armPackage/bin/arm-none-eabi-gcc.exe")) -and
    (Test-Path -LiteralPath (Join-Path $toolsRoot 'cmake-3.31.6-windows-x86_64/bin/cmake.exe')) -and
    (Test-Path -LiteralPath (Join-Path $toolsRoot 'ninja-1.12.1/ninja.exe'))) {
    & (Join-Path $PSScriptRoot 'trim-tools.ps1') -RemoveDownloadCache
}
Write-Host "Environment saved: $toolsRoot/environment.json"
Write-Host 'Setup complete. Reload VS Code, or run scripts/build.ps1 Debug.'
Write-Host 'For direct cmake commands in this terminal: . ./scripts/env.ps1'
