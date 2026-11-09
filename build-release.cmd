@echo off
call "%~dp0build.cmd" Release %*
exit /b %ERRORLEVEL%
