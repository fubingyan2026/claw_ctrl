@echo off
setlocal
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\package-portable.ps1" %*
set "packageResult=%ERRORLEVEL%"
echo.
if not "%packageResult%"=="0" echo Packaging failed. See the error above.
if not defined CLAW_BUILD_NO_PAUSE pause
exit /b %packageResult%
