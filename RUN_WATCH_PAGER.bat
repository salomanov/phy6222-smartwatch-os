@echo off
chcp 65001 > nul
title LP715 Smartwatch Pager
cd /d "%~dp0"
python watch_pager.py
if errorlevel 1 (
    echo.
    echo [ERROR] An error occurred while running watch_pager.py
    pause
)
