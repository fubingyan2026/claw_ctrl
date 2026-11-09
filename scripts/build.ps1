param(
    [ValidateSet('Debug','Release')][string]$Configuration='Debug',
    [switch]$Fresh,
    [switch]$Clean,
    [ValidateSet('all','memory')][string]$Target='all'
)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'env.ps1')
Push-Location $projectRoot
try {
    $configureArgs = @('--preset', $Configuration)
    $cache = Join-Path $projectRoot "build/$Configuration/CMakeCache.txt"
    if (Test-Path -LiteralPath $cache) {
        $previousSourceLine = Get-Content -LiteralPath $cache -Encoding UTF8 | Where-Object { $_ -match '^CMAKE_HOME_DIRECTORY:INTERNAL=' } | Select-Object -First 1
        if ($previousSourceLine) {
            $previousSource = ($previousSourceLine -split '=',2)[1]
            if ($previousSource.Replace('\','/') -ine $projectRoot.Replace('\','/')) {
                Write-Host 'Project moved: refreshing the CMake configuration.'
                $Fresh = $true
            }
        }
        $compilerLine = Get-Content -LiteralPath $cache -Encoding UTF8 | Where-Object { $_ -match '^CMAKE_C_COMPILER:(FILEPATH|STRING)=' } | Select-Object -First 1
        if ($compilerLine) {
            $previousCompiler = ($compilerLine -split '=',2)[1]
            $selectedCompiler = Join-Path $clawTools.ArmGccBin 'arm-none-eabi-gcc.exe'
            if ($previousCompiler.Replace('\','/') -ine $selectedCompiler.Replace('\','/')) {
                Write-Host 'Compiler changed: refreshing the CMake configuration.'
                $Fresh = $true
            }
        }
    }
    if ($Fresh) { $configureArgs += '--fresh' }
    & $clawTools.CMakeExe @configureArgs
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
    $buildArgs = @('--build', '--preset', $Configuration, '--parallel', '--target', $Target)
    if ($Clean) { $buildArgs += '--clean-first' }
    & $clawTools.CMakeExe @buildArgs
    if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
} finally { Pop-Location }
