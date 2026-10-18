@echo off
chcp 65001 >nul
python "%~dp0flash_firmware.py" %*
pause
