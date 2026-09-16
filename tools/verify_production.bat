@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0verify_production.ps1" %*
exit /b %ERRORLEVEL%
