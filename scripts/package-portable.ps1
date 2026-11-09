# Create an offline Windows source + tools bundle; preserve toolchain license files.
[CmdletBinding()]
param([switch]$Offline, [switch]$FullTools)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
try {
    & (Join-Path $PSScriptRoot 'setup-env.ps1') -PortableBuildTools -Offline:$Offline -KeepFullTools:$FullTools
    if($FullTools -and !(Test-Path -LiteralPath (Join-Path $projectRoot '.tools/arm-gnu-toolchain-14.2.rel1-mingw-w64-i686-arm-none-eabi/bin/arm-none-eabi-g++.exe'))) {
        throw 'Full SDK was trimmed. Re-extract the official SDK before requesting -FullTools.'
    }
    $dist=Join-Path $projectRoot 'dist'
    New-Item -ItemType Directory -Force $dist | Out-Null
    $kind=if($FullTools){'full'}else{'slim'}
    $archivePath=Join-Path $dist ("claw_ctrl-portable-$kind-win-x64-" + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '.zip')
    $excluded=@('.git','.tools','build','tmp','dist')
    $files=@(Get-ChildItem -LiteralPath $projectRoot -Force | Where-Object {
        $_.Name -notin $excluded -and $_.Name -ne 'CMakeUserPresets.json'
    } | ForEach-Object {
        if ($_.PSIsContainer) { Get-ChildItem -LiteralPath $_.FullName -Recurse -File -Force }
        else { $_ }
    })
    . (Join-Path $PSScriptRoot 'portable-tools.ps1')
    $files+=@(Get-PortableToolFiles -ProjectRoot $projectRoot -FullTools:$FullTools)
    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive=[IO.Compression.ZipFile]::Open($archivePath,[IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($file in $files) {
            $relative=$file.FullName.Substring($projectRoot.Length+1).Replace('\','/')
            $null=[IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,$file.FullName,$relative,[IO.Compression.CompressionLevel]::Optimal)
        }
    } finally { $archive.Dispose() }
    Write-Host "Offline portable package: $archivePath"
    Write-Host 'Extract it on Windows x64, then double-click build.cmd (Debug) or build-release.cmd (Release).'
    exit 0
} catch {
    Write-Host "Packaging failed: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
