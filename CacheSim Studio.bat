@echo off
REM CacheSim Studio.bat - double-click to open CacheSim Studio in its own window.
REM The first time, it also puts a "CacheSim Studio" shortcut on your Desktop.
powershell -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "%~dp0scripts\launch_studio.ps1"
if errorlevel 1 start "" "%~dp0web\index.html"
