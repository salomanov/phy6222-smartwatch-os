@echo off
chcp 65001 > nul
title Диагностика UART CH340 <-> PHY6212
python "%~dp0diag_uart.py"
pause
