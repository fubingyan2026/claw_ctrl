[CmdletBinding()]
param(
    [ValidateSet('Debug','Release')][string]$Configuration='Debug',
    [switch]$Offline,
    [switch]$Clean
)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
try {
    Write-Host "Project: $projectRoot"
    Write-Host '[1/2] Preparing project-local tools...'
    # Always regenerate machine-local paths, including after copying the project.
    & (Join-Path $PSScriptRoot 'setup-env.ps1') -PortableBuildTools -Offline:$Offline
    Write-Host "[2/2] Building $Configuration..."
    & (Join-Path $PSScriptRoot 'build.ps1') -Configuration $Configuration -Clean:$Clean
    Write-Host "Build succeeded. Output: $projectRoot/build/$Configuration/claw_ctrl.bin"
    exit 0
} catch {
    Write-Host "Build failed: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
