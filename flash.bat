@echo off
cd /d "%~dp0"
title Flashing PHY6222 v1.5
color 0B

echo ====================================================================
echo         FLASHING PHY6222 LP715 (v1.5 + SRAM OTA Installer)
echo ====================================================================
echo.
echo PORT: COM5 (115200 baud)
echo FILE: firmware\build\custom_firmware.hex
echo.
echo [!] INSTRUCTION:
echo   1. Ensure TM pin is connected to 3.3V (VCC).
echo   2. Briefly reconnect 3.3V power to VCC3V (or short RST to GND).
echo.
pause

echo.
echo [*] Writing Flash...
python tools\rdwr_phy62x2.py -p COM5 -b 115200 -t wh firmware\build\custom_firmware.hex

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ====================================================================
    echo [+++] SUCCESS! FIRMWARE v1.5 WRITTEN!
    echo ====================================================================
    echo 1. Disconnect TM from 3.3V.
    echo 2. Reconnect power (Reset).
    echo The watch will boot into clean diagnostic mode:
    echo    - CLICK: VIB SCANNER (P14 active, test pulse 350ms)
    echo    - HOLD: OTA MODE (supports true wireless In-App OTA updates)
    echo.
) else (
    echo.
    echo [!] Flashing error. Check TM pin (must be 3.3V) and do Reset again.
    echo.
)

pause
