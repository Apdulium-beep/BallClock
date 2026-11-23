@echo off
setlocal
cd /d "%~dp0firmware"
call pio run
if errorlevel 1 exit /b 1
copy /y .pio\build\d1_mini\firmware.bin firmware.bin
echo Firmware built. This command does NOT upload to the clock.
