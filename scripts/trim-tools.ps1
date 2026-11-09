# Remove only files excluded by the same allowlist used by the slim offline pack.
[CmdletBinding(SupportsShouldProcess=$true)]
param([switch]$RemoveDownloadCache, [switch]$RemoveLegacyFlatCopy)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$toolsRoot=[IO.Path]::GetFullPath((Join-Path $projectRoot '.tools'))
. (Join-Path $PSScriptRoot 'portable-tools.ps1')
$keep=New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
foreach($file in Get-PortableToolFiles -ProjectRoot $projectRoot) { $null=$keep.Add($file.FullName) }
$targets=@('arm-gnu-toolchain-14.2.rel1-mingw-w64-i686-arm-none-eabi','cmake-3.31.6-windows-x86_64','ninja-1.12.1')
if($RemoveDownloadCache) { $targets+='downloads' }
if($RemoveLegacyFlatCopy) {
    # Older setup extracted a redundant SDK at .tools itself; never touch named SDKs here.
    $environmentFile=Join-Path $toolsRoot 'environment.json'
    if(Test-Path -LiteralPath $environmentFile) {
        $config=Get-Content -LiteralPath $environmentFile -Raw | ConvertFrom-Json
        if($config.ArmGccBin -and $config.ArmGccBin.Replace('\','/').TrimEnd('/') -ieq (Join-Path $toolsRoot 'bin').Replace('\','/')) {
            throw 'The flat SDK is currently active; select the named standalone SDK before removing it.'
        }
    }
    $targets+=@('arm-none-eabi','bin','include','lib','libexec','share','.version','manifest.txt')
}
$removedBytes=0L
$removedCount=0
foreach($name in $targets) {
    $target=[IO.Path]::GetFullPath((Join-Path $toolsRoot $name))
    if(!$target.StartsWith($toolsRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Cleanup target escapes .tools' }
    if(!(Test-Path -LiteralPath $target)) { continue }
    $item=Get-Item -LiteralPath $target -Force
    if($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Refusing to trim a linked directory: $target" }
    $files=if($item.PSIsContainer){@(Get-ChildItem -LiteralPath $target -Recurse -File -Force)}else{@($item)}
    $redundant=New-Object 'System.Collections.Generic.List[System.IO.FileInfo]'
    foreach($file in $files) {
        if(!$file.FullName.StartsWith($target,[StringComparison]::OrdinalIgnoreCase)) { throw 'Unexpected file outside cleanup target' }
        if(!$keep.Contains($file.FullName)) { $redundant.Add($file) }
    }
    if($redundant.Count -gt 0 -and $PSCmdlet.ShouldProcess($target,"Remove $($redundant.Count) redundant tool files")) {
        $removedBytes+=($redundant | Measure-Object Length -Sum).Sum
        $removedCount+=$redundant.Count
        Remove-Item -LiteralPath @($redundant.FullName) -Force
    }
    if($item.PSIsContainer -and !$WhatIfPreference) {
        Get-ChildItem -LiteralPath $target -Directory -Recurse -Force | Sort-Object { $_.FullName.Length } -Descending | ForEach-Object {
            if(!($_.Attributes -band [IO.FileAttributes]::ReparsePoint) -and @(Get-ChildItem -LiteralPath $_.FullName -Force).Count -eq 0) {
                Remove-Item -LiteralPath $_.FullName -Force
            }
        }
        # Setup may retain a directory handle for downloads; an empty cache costs no payload space.
        if($name -ne 'downloads' -and @(Get-ChildItem -LiteralPath $target -Force).Count -eq 0) { Remove-Item -LiteralPath $target -Force }
    }
}
Write-Host ("Removed {0} redundant files, {1:N1} MiB." -f $removedCount,($removedBytes/1MB))
