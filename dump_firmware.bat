@echo off
chcp 65001 > nul
title Снятие заводского дампа 512KB - PHY6222
python "%~dp0dump_firmware.py" %*
pause
