$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
Push-Location $projectRoot
try {
    New-Item -ItemType Directory -Force build/host-tests | Out-Null
    wsl -d Ubuntu --exec gcc -std=c11 -Wall -Wextra -Werror "-fsanitize=address,undefined" -g -Iservice -Idevice_drivers -Im_middlewares/utils tests/test_services.c service/srv_juxie.c service/srv_indicator.c m_middlewares/utils/msg_fifo.c -o build/host-tests/test_services
    if($LASTEXITCODE -ne 0) { throw 'Host test compilation failed (requires Ubuntu WSL gcc)' }
    wsl -d Ubuntu --exec ./build/host-tests/test_services
    if($LASTEXITCODE -ne 0) { throw 'Host tests failed' }
} finally { Pop-Location }

