@echo off
chcp 65001 > nul
title PHY6212 Умный опрос (115200 / 9600)
python "%~dp0smart_probe.py"
pause
