import serial
import serial.tools.list_ports
import time
import sys
import subprocess

def get_port():
    ports = list(serial.tools.list_ports.comports())
    for p in ports:
        if "CH34" in p.description or "USB" in p.hwid or "VID" in p.hwid:
            if "BTHENUM" not in p.hwid:
                return p.device
    return "COM5"

port = get_port()
print("=" * 65)
print(f"[*] ГЛУБОКИЙ ПЕРЕБОР BOOTLOADER PHY6212 (Порт {port})")
print("=" * 65)
print("[*] TM = 3.3V (подключен)")
print("[*] Команды инициализации: UXTL16, UXTDWU, UDLL48")
print("[*] Скорости: 115200 <-> 9600")
print("[*] Время работы: 60 секунд")
print("-" * 65)
print("[*] СЕЙЧАС: кратковременно чиркайте EN на GND или передерните 3.3V!")
print("=" * 65)

cmds = [b"UXTL16", b"UXTDWU", b"UDLL48"]
bauds = [115200, 9600]

t_end = time.time() + 60.0
last_switch = time.time()
baud_idx = 0
total_rx = 0
found = False

try:
    s = serial.Serial(port, bauds[baud_idx], timeout=0.03)
    s.reset_input_buffer()
    
    last_ping = 0
    cmd_idx = 0
    
    while time.time() < t_end:
        now = time.time()
        rem = int(t_end - now)
        
        # Каждые 3 секунды меняем скорость 115200 <-> 9600
        if now - last_switch > 3.0:
            baud_idx = (baud_idx + 1) % len(bauds)
            s.baudrate = bauds[baud_idx]
            s.reset_input_buffer()
            last_switch = now
            sys.stdout.write(f"\r[*] Скорость {s.baudrate:6d} бод | Осталось {rem:2d}с | Ждём сброс EN / питания...   ")
            sys.stdout.flush()
            
        # Каждые 50 мс шлём одну из команд
        if now - last_ping > 0.05:
            current_cmd = cmds[cmd_idx]
            s.write(current_cmd)
            cmd_idx = (cmd_idx + 1) % len(cmds)
            last_ping = now
            
        data = s.read(128)
        if data:
            total_rx += len(data)
            hex_str = " ".join(f"{b:02X}" for b in data)
            print(f"\n\n[!!!] ОТКЛИК ПОЛУЧЕН ({len(data)} байт, {s.baudrate} бод)!: {data}")
            print(f"      HEX: {hex_str}")
            if b"cmd>>:" in data or b"fct>>:" in data or b"OK" in data:
                print(f"\n[+++] БИНГО! Получен отклик бутлоадера на {s.baudrate} бод!")
                found = True
                break
                
        time.sleep(0.01)
        
    s.close()
    print("\n" + "=" * 65)
    if found:
        print("[+] Чип вошёл в режим загрузчика! Запускаем считывание дампа...")
    else:
        print(f"[-] Завершено. Всего принято байт: {total_rx}")
        if total_rx == 0:
            print("[-] Внимание: 0 байт. Попробуйте поменять местами TX и RX на адаптере!")
    print("=" * 65)

except Exception as e:
    print(f"\n[!] Ошибка порта: {e}")
