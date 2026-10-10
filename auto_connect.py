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

print("=" * 70)
print(f"[*] АВТОМАТИЧЕСКИЙ ЗАХВАТ BOOTLOADER PHY6222 (ПОРТ {port})")
print("=" * 70)
print("[*] СОВЕТ ПО ПОДКЛЮЧЕНИЮ:")
print("    Соедините проводки от пятаков TM и VCC3V ВМЕСТЕ в один провод +3.3V!")
print("    (Чтобы не было паразитной подпитки чипа через TM).")
print("=" * 70)

script_path = os.path.join(os.path.dirname(__file__), "tools", "rdwr_phy62x2.py")

print("\n--- ПОПЫТКА 1: Стандартная скорость 9600 бод (чип с завода ждёт 9600) ---")
print("[!] СЕЙЧАС ПОДКЛЮЧИТЕ ПРОВОД 3.3V (TM+VCC3V) К ПИТАНИЮ 3.3V АДАПТЕРА!\n")
time.sleep(1)

# Без -t -> 9600 бод
cmd1 = [sys.executable, script_path, "-p", port, "i"]
res1 = subprocess.run(cmd1)

if res1.returncode == 0:
    print("\n[+++] УСПЕХ НА 9600 БОД!")
    sys.exit(0)

print("\n----------------------------------------------------------------------")
print("[-] На 9600 бод ответа нет. Пробуем ПОПЫТКУ 2 на 115200 бод (-t)...")
print("[!] ОТКЛЮЧИТЕ И СНОВА ПОДКЛЮЧИТЕ провод 3.3V (TM+VCC3V)!\n")
time.sleep(2)

# С -t -> 115200 бод
cmd2 = [sys.executable, script_path, "-p", port, "-b", "115200", "-t", "i"]
res2 = subprocess.run(cmd2)

if res2.returncode == 0:
    print("\n[+++] УСПЕХ НА 115200 БОД!")
    sys.exit(0)

print("\n======================================================================")
print("[-] Если ответа нет в обеих попытках:")
print("    1. Поменяйте местами проводки TX и RX на адаптере CH340")
print("    2. Запустите этот скрипт ещё раз!")
print("======================================================================")
sys.exit(1)
