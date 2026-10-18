@echo off
chcp 65001 > nul
title UART Live Monitor - PHY6212
python "%~dp0listen_uart.py" %*
pause
