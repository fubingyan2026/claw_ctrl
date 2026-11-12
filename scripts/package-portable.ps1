# Create an offline Windows source + tools bundle; preserve toolchain license files.
[CmdletBinding()]
param([switch]$Offline, [switch]$FullTools, [switch]$FlashOnly)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
try {
    $dist=Join-Path $projectRoot 'dist'
    New-Item -ItemType Directory -Force $dist | Out-Null
    $entries = [ordered]@{}
    if ($FlashOnly) {
        & (Join-Path $PSScriptRoot 'one-click-build.ps1') -Configuration Release -Offline:$Offline
        if ($LASTEXITCODE -ne 0) { throw 'Release build failed.' }
        $packageName = 'claw_ctrl-factory-flash'
        foreach ($file in 'claw_factory.bin', 'claw_factory.jlink') {
            $entries[$file] = Join-Path $projectRoot "build/Release/$file"
        }
        foreach ($file in 'flash-factory.cmd', 'scripts/flash-factory.ps1') {
            $entries[$file] = Join-Path $projectRoot $file
        }
    } else {
        & (Join-Path $PSScriptRoot 'setup-env.ps1') -PortableBuildTools -Offline:$Offline -KeepFullTools:$FullTools
        if($FullTools -and !(Test-Path -LiteralPath (Join-Path $projectRoot '.tools/arm-gnu-toolchain-14.2.rel1-mingw-w64-i686-arm-none-eabi/bin/arm-none-eabi-g++.exe'))) {
            throw 'Full SDK was trimmed. Re-extract the official SDK before requesting -FullTools.'
        }
        $kind=if($FullTools){'full'}else{'slim'}
        $packageName = "claw_ctrl-portable-$kind"
        $excluded=@('.git','.tools','build','tmp','dist')
        $files=@(Get-ChildItem -LiteralPath $projectRoot -Force | Where-Object {
            $_.Name -notin $excluded -and $_.Name -ne 'CMakeUserPresets.json'
        } | ForEach-Object {
            if ($_.PSIsContainer) { Get-ChildItem -LiteralPath $_.FullName -Recurse -File -Force }
            else { $_ }
        })
        . (Join-Path $PSScriptRoot 'portable-tools.ps1')
        $files+=@(Get-PortableToolFiles -ProjectRoot $projectRoot -FullTools:$FullTools)
        foreach ($file in $files) {
            $relative=$file.FullName.Substring($projectRoot.Length+1).Replace('\','/')
            $entries[$relative] = $file.FullName
        }
    }
    $archivePath=Join-Path $dist ("$packageName-win-x64-" + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '.zip')
    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive=[IO.Compression.ZipFile]::Open($archivePath,[IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($entry in $entries.GetEnumerator()) {
            $null=[IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,$entry.Value,$entry.Key,[IO.Compression.CompressionLevel]::Optimal)
        }
        if ($FlashOnly) {
            $readme = [IO.StreamWriter]::new($archive.CreateEntry('README.txt').Open())
            try {
                $readme.WriteLine('Extract all files. Install SEGGER J-Link if needed: https://www.segger.com/downloads/jlink/')
                $readme.WriteLine('Power the board, connect J-Link SWD, then double-click flash-factory.cmd.')
                $readme.WriteLine('Factory flashing erases all flash, including existing parameters and boot counts.')
            } finally { $readme.Dispose() }
        }
    } finally { $archive.Dispose() }
    Write-Host "Offline portable package: $archivePath"
    if ($FlashOnly) {
        Write-Host 'Extract it on Windows x64, then double-click flash-factory.cmd with J-Link installed.'
    } else {
        Write-Host 'Extract it on Windows x64, then double-click build.cmd (Debug) or build-release.cmd (Release).'
    }
    exit 0
} catch {
    Write-Host "Packaging failed: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
