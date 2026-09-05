@echo off
rem ===========================================================================
rem Capture all six tabs to PNG + one heap summary for visual review.
rem
rem   tools\shoot_all.bat                  ^> PNGs + heap_summary.txt into
rem                                          docs\screenshots (demo, stopped)
rem   tools\shoot_all.bat -Telemetry demo-run   ^> motor spinning, livelier shots
rem   tools\shoot_all.bat -OutDir C:\tmp\shots  ^> keep a working copy elsewhere
rem   tools\shoot_all.bat -SkipBuild            ^> reuse the existing simulator
rem
rem Requires: a built simulator (see build_sim.bat) + Python with Pillow.
rem See tools/README.md for the full list of options.
rem ===========================================================================
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0shoot_all.ps1" %*
exit /b %ERRORLEVEL%
