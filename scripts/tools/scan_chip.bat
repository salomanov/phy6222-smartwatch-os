@echo off
chcp 65001 > nul
title PHY6212 - Сканирование и подключение
python "%~dp0scan_chip.py" %*
pause
