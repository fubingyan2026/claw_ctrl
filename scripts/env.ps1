# Dot-source this file to activate the project tools in the current terminal.
$environmentFile = Join-Path (Split-Path $PSScriptRoot -Parent) '.tools/environment.json'
if (!(Test-Path -LiteralPath $environmentFile)) {
    throw 'Project environment missing. Run: powershell -NoProfile -ExecutionPolicy Bypass -File scripts/setup-env.ps1'
}
$clawTools = Get-Content -LiteralPath $environmentFile -Raw | ConvertFrom-Json
foreach ($file in @((Join-Path $clawTools.ArmGccBin 'arm-none-eabi-gcc.exe'), $clawTools.CMakeExe, $clawTools.NinjaExe)) {
    if (!(Test-Path -LiteralPath $file -PathType Leaf)) { throw "Tool missing: $file. Rerun scripts/setup-env.ps1." }
}
$env:ARM_GCC_PATH = $clawTools.ArmGccBin
$clawToolDirs = @($clawTools.ArmGccBin, (Split-Path $clawTools.CMakeExe -Parent), (Split-Path $clawTools.NinjaExe -Parent)) | Select-Object -Unique
$clawExistingPath = @($env:PATH -split ';' | Where-Object { $_ -and $_ -notin $clawToolDirs })
$env:PATH = (@($clawToolDirs) + $clawExistingPath) -join ';'
