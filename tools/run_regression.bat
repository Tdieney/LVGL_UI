@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0run_regression.ps1" %*
exit /b %ERRORLEVEL%
