import serial
import serial.tools.list_ports
import time
import sys

def find_port():
    if len(sys.argv) > 1:
        return sys.argv[1]
    ports = list(serial.tools.list_ports.comports())
    # Ищем CH340 / USB-UART
    for p in ports:
        if "CH34" in p.description or "USB" in p.hwid or "VID" in p.hwid:
            if "BTHENUM" not in p.hwid:
                return p.device
    for p in ports:
        if p.device == "COM5":
            return "COM5"
    return "COM5"

port = find_port()
print("=" * 60)
print(f"[*] ЖИВОЙ МОНИТОР UART: Порт {port}, Скорость 115200")
print("=" * 60)
print("[*] Сейчас непрерывно слушаем входящие данные от чипа.")
print("[*] Сделайте сброс: кратковременно коснитесь EN на массу (GND).")
print("[*] Если связь есть — здесь сразу побегут байты ответа.")
print("[*] Для выхода нажмите Ctrl + C.")
print("-" * 60)

try:
    with serial.Serial(port, 115200, timeout=0.05) as s:
        s.reset_input_buffer()
        s.reset_output_buffer()
        
        last_ping = time.time()
        ping_count = 0
        while True:
            now = time.time()
            # Каждые 120 мс посылаем команду опроса загрузчика UXTDWU
            if now - last_ping > 0.12:
                s.write(b"UXTDWU")
                last_ping = now
                ping_count += 1
                if ping_count % 25 == 0:
                    print(f"[{time.strftime('%H:%M:%S')}] Запрос отправляется... Ждём ответ (коснитесь EN)...")
                
            data = s.read(128)
            if data:
                hex_str = " ".join(f"{b:02X}" for b in data)
                print(f"\n[+] ПРИНЯТО {len(data)} БАЙТ: {data}")
                print(f"    HEX: {hex_str}\n")
            time.sleep(0.01)
except KeyboardInterrupt:
    print("\nМонитор остановлен.")
except Exception as e:
    print(f"\n[!] Ошибка открытия порта {port}: {e}")
