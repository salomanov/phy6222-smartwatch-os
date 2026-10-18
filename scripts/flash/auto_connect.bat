@echo off
chcp 65001 > nul
title Авто-захват PHY6222 (9600 и 115200)
python "%~dp0auto_connect.py" %*
pause
