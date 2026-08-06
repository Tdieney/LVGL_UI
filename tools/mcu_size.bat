@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0mcu_size.ps1" %*
exit /b %ERRORLEVEL%
