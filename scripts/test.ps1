$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
Push-Location $projectRoot
try {
    New-Item -ItemType Directory -Force build/host-tests | Out-Null
    wsl -d Ubuntu --exec gcc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter "-fsanitize=address,undefined" -g -ICore/Inc -Iservice -Idevice_drivers -Im_middlewares/algorithm/filter -Im_middlewares/algorithm/math tests/test_board_protocol.c service/srv_board_protocol.c service/srv_juxie.c service/srv_fill_light.c m_middlewares/algorithm/filter/filter.c m_middlewares/algorithm/math/maths.c -lm -o build/host-tests/test_board_protocol
    if($LASTEXITCODE -ne 0) { throw 'Protocol test compilation failed (requires Ubuntu WSL gcc)' }
    wsl -d Ubuntu --exec ./build/host-tests/test_board_protocol
    if($LASTEXITCODE -ne 0) { throw 'Protocol tests failed' }
    wsl -d Ubuntu --exec gcc -std=c11 -Wall -Wextra -Werror "-fsanitize=address,undefined" -g -DPRINTF_DISABLE_SUPPORT_FLOAT -DPRINTF_DISABLE_SUPPORT_EXPONENTIAL -Im_middlewares/log -Im_middlewares/utils -Im_middlewares/Third_Party/mpaland_printf tests/test_middleware_port.c m_middlewares/log/log.c m_middlewares/utils/kfifo.c m_middlewares/Third_Party/mpaland_printf/printf.c -o build/host-tests/test_middleware_port
    if($LASTEXITCODE -ne 0) { throw 'Middleware test compilation failed' }
    wsl -d Ubuntu --exec ./build/host-tests/test_middleware_port
    if($LASTEXITCODE -ne 0) { throw 'Middleware tests failed' }
    wsl -d Ubuntu --exec gcc -std=c11 -Wall -Wextra -Werror "-fsanitize=address,undefined" -g -Iservice -Idevice_drivers tests/test_indicator.c service/srv_indicator.c service/srv_juxie.c -o build/host-tests/test_indicator
    if($LASTEXITCODE -ne 0) { throw 'Indicator test compilation failed' }
    wsl -d Ubuntu --exec ./build/host-tests/test_indicator
    if($LASTEXITCODE -ne 0) { throw 'Indicator tests failed' }
    if(Test-Path -LiteralPath tests/test_services.c) {
        wsl -d Ubuntu --exec gcc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter "-fsanitize=address,undefined" -g -Iservice -Idevice_drivers -Im_middlewares/utils tests/test_services.c service/srv_juxie.c service/srv_indicator.c m_middlewares/utils/msg_fifo.c -o build/host-tests/test_services
        if($LASTEXITCODE -ne 0) { throw 'Service test compilation failed' }
        wsl -d Ubuntu --exec ./build/host-tests/test_services
        if($LASTEXITCODE -ne 0) { throw 'Service tests failed' }
    }
} finally { Pop-Location }
