@echo off
cd /d "%~dp0"
title Flashing PHY6222 v1.4
color 0B

echo ====================================================================
echo         FLASHING PHY6222 LP715 (v1.4 + In-App OTA)
echo ====================================================================
echo.
echo PORT: COM5 (115200 baud)
echo FILE: firmware\build\custom_firmware.hex
echo.
echo [!] ATTENTION:
echo   1. Ensure TM pin is connected to 3.3V (VCC).
echo   2. Short RST to GND briefly (or reconnect power).
echo.
pause

echo.
echo [*] Writing Flash...
python tools\rdwr_phy62x2.py -p COM5 -b 115200 -t wh firmware\build\custom_firmware.hex

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ====================================================================
    echo [+++] SUCCESS! FIRMWARE WRITTEN!
    echo ====================================================================
    echo Now disconnect TM from 3.3V and press Reset.
    echo The watch is now ready for wireless In-App OTA!
    echo.
) else (
    echo.
    echo [!] Flashing error. Check TM pin (must be 3.3V) and do Reset again.
    echo.
)

pause
