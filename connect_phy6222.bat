@echo off
chcp 65001 > nul
title Подключение PHY6222 через подачу питания
python "%~dp0connect_phy6222.py" %*
pause
