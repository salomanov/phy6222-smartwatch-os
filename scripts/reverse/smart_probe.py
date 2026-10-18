import serial
import serial.tools.list_ports
import time
import sys

def get_port():
    ports = list(serial.tools.list_ports.comports())
    for p in ports:
        if "CH34" in p.description or "USB" in p.hwid or "VID" in p.hwid:
            if "BTHENUM" not in p.hwid:
                return p.device
    return "COM5"

port = get_port()
print("=" * 65)
print(f"[*] УМНЫЙ СКАНЕР ПОДКЛЮЧЕНИЯ PHY6212 (Порт {port})")
print("=" * 65)
print("[*] Пин 8 чипа = TX (к RX CH340)")
print("[*] Пин 9 чипа = RX (к TX CH340)")
print("[*] TM = 3.3V")
print("-" * 65)
print("[!] СПОСОБЫ СБРОСА ЧИПА В БУТЛОАДЕР:")
print("    1. Кратковременно чиркнуть EN на GND")
print("    2. ИЛИ: выдернуть и сразу воткнуть провод 5V (VBUS) - сброс питанием!")
print("=" * 65)

bauds = [115200, 9600]
t_end = time.time() + 60.0

try:
    s = serial.Serial(port, 115200, timeout=0.03)
    current_baud_idx = 0
    last_switch = time.time()
    last_ping = 0
    total_rx = 0
    success = False
    
    print(f"[*] Сканирование активно (60 сек)... Текущая скорость: {bauds[current_baud_idx]}")
    
    while time.time() < t_end:
        now = time.time()
        
        # Каждые 3 секунды чередуем скорость 115200 <-> 9600 на случай, если чип ждёт 9600
        if now - last_switch > 3.0:
            current_baud_idx = (current_baud_idx + 1) % len(bauds)
            s.baudrate = bauds[current_baud_idx]
            s.reset_input_buffer()
            last_switch = now
            sys.stdout.write(f"\r[*] Переключение на {bauds[current_baud_idx]} бод... Ждём сброс EN или передергивание 5V...  ")
            sys.stdout.flush()
            
        # Шлём рукопожатие каждые 70 мс
        if now - last_ping > 0.07:
            s.write(b"UXTDWU")
            last_ping = now
            
        data = s.read(128)
        if data:
            total_rx += len(data)
            hex_str = " ".join(f"{b:02X}" for b in data)
            print(f"\n\n[!!!] ПРИНЯТЫ БАЙТЫ ({len(data)} шт, {s.baudrate} бод): {data}")
            print(f"      HEX: {hex_str}")
            if b"cmd>>:" in data or b"fct>>:" in data:
                print(f"\n[+++] УСПЕХ! Чип ответил на скорости {s.baudrate} бод!")
                success = True
                break
                
        time.sleep(0.01)
        
    s.close()
    print("\n" + "=" * 65)
    if success:
        print("[+] Чип готов к снятию дампа!")
    else:
        print(f"[-] Результат: принято {total_rx} байт.")
        if total_rx == 0:
            print("[-] Внимание: 0 байт! Проверьте:")
            print("    1. Землю (GND адаптера соединён с GND часов)")
            print("    2. Попробуйте сброс ПЕРЕДЁРГИВАНИЕМ ПИТАНИЯ (отключить и подключить 5V к VBUS)")
            print("    3. Проверьте петлю TX-RX на самом адаптере (жив ли адаптер)")
    print("=" * 65)

except Exception as e:
    print(f"\n[!] Ошибка порта {port}: {e}")
