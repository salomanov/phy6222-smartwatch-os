import sys
import time
import serial
import serial.tools.list_ports

def find_serial_port():
    ports = list(serial.tools.list_ports.comports())
    usb_ports = [p for p in ports if "BTHENUM" not in p.hwid and ("USB" in p.hwid or "VID" in p.hwid)]
    if usb_ports:
        return usb_ports[0].device
    for p in ports:
        if p.device == "COM5":
            return "COM5"
    return "COM5"

port = sys.argv[1] if len(sys.argv) > 1 else find_serial_port()
print(f"[*] Открываем порт {port} для мониторинга...")
print("[*] Сейчас проверяем: шлёт ли плата хоть какие-то данные при включении/сбросе.\n")

for baud in [115200, 9600]:
    print(f"--- Тест на скорости {baud} бод (слушаем 3 секунды) ---")
    print(">>> Пожалуйста, замкните пятак EN на GND на полсекунды (сделайте сброс)!")
    try:
        with serial.Serial(port, baud, timeout=0.1) as s:
            t_end = time.time() + 3.0
            rx_data = bytearray()
            while time.time() < t_end:
                b = s.read(64)
                if b:
                    rx_data.extend(b)
                    print(f"Принято: {b}")
            if not rx_data:
                print(f"Тишина на {baud} (0 байт получено).")
            else:
                print(f"ИТОГО на {baud}: {rx_data}")
    except Exception as e:
        print(f"Ошибка порта: {e}")
    print()
