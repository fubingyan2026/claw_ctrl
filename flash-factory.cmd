@echo off
setlocal
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\flash-factory.ps1" %*
set "flashResult=%ERRORLEVEL%"
echo.
if not "%flashResult%"=="0" echo Flash failed. See the error above.
if not defined CLAW_FLASH_NO_PAUSE pause
exit /b %flashResult%
