import sys
import os
import subprocess
import serial
import serial.tools.list_ports
import time

def find_serial_port():
    ports = list(serial.tools.list_ports.comports())
    for p in ports:
        if "CH34" in p.description or "USB" in p.hwid or "VID" in p.hwid:
            if "BTHENUM" not in p.hwid:
                return p.device
    return "COM5"

port = sys.argv[1] if len(sys.argv) > 1 else find_serial_port()

print("=" * 68)
print(f"[*] ПОДКЛЮЧЕНИЕ К ЧИПУ PHY6222 (ПОРТ {port})")
print("=" * 68)
print("[*] СХЕМА ПОДКЛЮЧЕНИЯ:")
print("    - GND адаптера -> пятак GND")
print("    - TX адаптера  -> пятак RX (ножка 10/9)")
print("    - RX адаптера  -> пятак TX (ножка 9/8)")
print("    - TM часов     -> 3.3V (подключен постоянно!)")
print("    - VCC3V        -> провод 3.3V держим в руке, ПОКА НЕ ПОДКЛЮЧАЕМ")
print("=" * 68)
print("[!] ИНСТРУКЦИЯ:")
print("    Сейчас начнётся опрос подключения.")
print("    КАК ТОЛЬКО увидите надпись 'ПОДАЙТЕ ПИТАНИЕ' —")
print("    ПОДКЛЮЧИТЕ провод 3.3V к пятаку VCC3V!")
print("=" * 68)
print()

# Запускаем официальный dumper rdwr_phy62x2.py с флагом -t (TM=1) и командой i (info)
script_path = os.path.join(os.path.dirname(__file__), "tools", "rdwr_phy62x2.py")
cmd = [sys.executable, script_path, "-p", port, "-b", "115200", "-t", "i"]

print("[>>>] ПОДАЙТЕ ПИТАНИЕ 3.3V НА VCC3V ПРЯМО СЕЙЧАС! [<<<]\n")
res = subprocess.run(cmd)
sys.exit(res.returncode)
