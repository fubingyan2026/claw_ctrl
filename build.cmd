@echo off
setlocal
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\one-click-build.ps1" %*
set "buildResult=%ERRORLEVEL%"
echo.
if not "%buildResult%"=="0" echo Build failed. See the error above.
if not defined CLAW_BUILD_NO_PAUSE pause
exit /b %buildResult%
