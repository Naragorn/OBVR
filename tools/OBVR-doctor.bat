@echo off
rem OBVR doctor: double-click this file. It checks your OBVR install and changes nothing.
rem It runs OBVR-doctor.ps1 next to it; -ExecutionPolicy Bypass lets the script run
rem without changing any setting, for this one run only.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0OBVR-doctor.ps1" %*
echo.
pause
