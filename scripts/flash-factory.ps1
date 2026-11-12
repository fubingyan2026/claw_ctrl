[CmdletBinding()]
param(
    [ValidateSet('Debug','Release')][string]$Configuration = 'Release',
    [string]$JLinkExe,
    [switch]$Offline
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent

try {
    if (!$JLinkExe) {
        $localCommander = Join-Path $projectRoot '.tools/jlink/JLink.exe'
        $command = Get-Command 'JLink.exe' -CommandType Application -ErrorAction SilentlyContinue
        if (Test-Path -LiteralPath $localCommander -PathType Leaf) {
            $JLinkExe = $localCommander
        } elseif ($command) {
            $JLinkExe = $command.Source
        } else {
            $installed = foreach ($base in @($env:ProgramFiles, ${env:ProgramFiles(x86)}, $env:USERPROFILE) | Select-Object -Unique) {
                if ($base) {
                    Get-ChildItem -Path (Join-Path $base 'SEGGER/JLink*/JLink.exe') -File -ErrorAction SilentlyContinue
                }
            }
            $JLinkExe = ($installed | Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName
        }
    }
    if (!$JLinkExe -or !(Test-Path -LiteralPath $JLinkExe -PathType Leaf)) {
        throw 'JLink.exe was not found. Install SEGGER J-Link or pass -JLinkExe with its full path.'
    }
    $JLinkExe = (Resolve-Path -LiteralPath $JLinkExe).Path

    $imageDirectory = $projectRoot
    if (Test-Path -LiteralPath (Join-Path $projectRoot 'CMakeLists.txt') -PathType Leaf) {
        & (Join-Path $PSScriptRoot 'one-click-build.ps1') -Configuration $Configuration -Offline:$Offline
        if ($LASTEXITCODE -ne 0) { throw 'Firmware build failed.' }
        $imageDirectory = Join-Path $projectRoot "build/$Configuration"
    }

    Push-Location $imageDirectory
    try {
        foreach ($file in 'claw_factory.bin', 'claw_factory.jlink') {
            if (!(Test-Path -LiteralPath $file -PathType Leaf)) { throw "Missing factory package file: $file" }
        }
        Write-Host "J-Link: $JLinkExe"
        Write-Host 'Erasing all device flash, then programming and verifying claw_factory.bin.'
        & $JLinkExe -Device GD32F503KEU7 -If SWD -Speed 4000 -AutoConnect 1 -ExitOnError 1 -NoGui 1 -CommandFile claw_factory.jlink
        if ($LASTEXITCODE -ne 0) { throw "J-Link Commander exited with code $LASTEXITCODE." }
        Write-Host 'Factory firmware programmed and verified. The target has restarted.'
    } finally {
        Pop-Location
    }
    exit 0
} catch {
    Write-Host "Flash failed: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
