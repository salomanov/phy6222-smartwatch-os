import serial
import time
import sys

port = "COM5"
print("=" * 65)
print(f"[*] ТЕСТ НОРМАЛЬНОГО СТАРТА ЧАСОВ (ПОРТ {port})")
print("=" * 65)
print("[*] TM ОТКЛЮЧЕН!")
print("[*] Сейчас включите питание 3.3V на VCC3V (или чиркните EN на GND).")
print("[*] Часы должны включиться (зажечь экран).")
print("[*] Слушаем входящие данные 15 секунд...")
print("=" * 65)

try:
    with serial.Serial(port, 115200, timeout=0.05) as s:
        s.reset_input_buffer()
        t_end = time.time() + 15.0
        total = 0
        while time.time() < t_end:
            data = s.read(128)
            if data:
                total += len(data)
                hex_vals = " ".join(f"{b:02X}" for b in data)
                text = "".join(chr(b) if 32 <= b <= 126 else "." for b in data)
                print(f"\n[+] ПОЛУЧЕНО {len(data)} БАЙТ: {text}")
                print(f"    HEX: {hex_vals}")
            time.sleep(0.01)
        
        print("\n" + "=" * 65)
        if total == 0:
            print("[-] За 15 секунд получено 0 байт на 115200 бод.")
        else:
            print(f"[+] Всего получено: {total} байт! Линия передачи работает!")
        print("=" * 65)
except Exception as e:
    print(f"[!] Ошибка: {e}")
