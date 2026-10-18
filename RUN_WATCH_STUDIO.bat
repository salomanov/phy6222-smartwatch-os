@echo off
chcp 65001 >nul
title LP715 Smartwatch Studio
cd /d "%~dp0"
python watch_studio.py
pause
