@echo off
chcp 65001 >nul
python "%~dp0read_flash_dump.py" %*
pause
