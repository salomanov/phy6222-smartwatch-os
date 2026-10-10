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
print(f"[*] ДИАГНОСТИКА UART CH340 <-> ЧАСЫ НА ПОРТУ {port}")
print("=" * 65)
print("1 - Слушать нормальный старт часов (без TM, смотрим лог загрузки)")
print("2 - Проверить адаптер CH340 (Петля TX -> RX)")
print("3 - Непрерывный поиск Bootloader (с TM=3.3V, чиркаем EN об GND)")
print("=" * 65)

choice = input("Выберите режим (1, 2 или 3) [по умолчанию 1]: ").strip()
if not choice:
    choice = "1"

if choice == "2":
    print("\n[*] ТЕСТ АДАПТЕРА: Соедините на самом CH340 пин TX с пином RX проводком!")
    input("Нажмите Enter, когда замкнули TX и RX...")
    try:
        with serial.Serial(port, 115200, timeout=1) as s:
            test_msg = b"TEST_CH340_OK_12345"
            s.write(test_msg)
            time.sleep(0.05)
            rx = s.read(len(test_msg))
            if rx == test_msg:
                print(f"\n[+++] АДАПТЕР CH340 ПОЛНОСТЬЮ ИСПРАВЕН! Эхо получено: {rx.decode()}")
            else:
                print(f"\n[---] ОШИБКА АДАПТЕРА: Отправлено {test_msg}, получено {rx}")
    except Exception as e:
        print(f"[!] Ошибка порта: {e}")

elif choice == "1":
    print("\n[*] РЕЖИМ 1: СЛУШАЕМ ОБЫЧНЫЙ СТАРТ ЧАСОВ (БЕЗ TM)")
    print("[*] Убедитесь, что провод TM ОТКЛЮЧЕН от 3.3V!")
    print("[*] Включите питание или кратковременно чиркните EN на GND.")
    print("[*] Слушаем 115200 и 9600... Для выхода Ctrl+C.\n")
    try:
        with serial.Serial(port, 115200, timeout=0.05) as s:
            s.reset_input_buffer()
            t_start = time.time()
            total = 0
            while True:
                data = s.read(64)
                if data:
                    total += len(data)
                    hex_str = " ".join(f"{b:02X}" for b in data)
                    text = "".join(chr(b) if 32 <= b <= 126 else "." for b in data)
                    print(f"[{time.strftime('%H:%M:%S')}] Получено {len(data)} байт: {text}")
                    print(f"                 HEX: {hex_str}")
                time.sleep(0.01)
    except KeyboardInterrupt:
        print("\nОстановлено.")
    except Exception as e:
        print(f"[!] Ошибка: {e}")

elif choice == "3":
    print("\n[*] РЕЖИМ 3: БУТЛОАДЕР (TM подключен к 3.3V)")
    print("[*] Непрерывно посылаем 'UXTDWU' на 115200 бод.")
    print("[*] Кратковременно чиркните EN на GND (буквально на 0.05 сек)!\n")
    try:
        with serial.Serial(port, 115200, timeout=0.04) as s:
            s.reset_input_buffer()
            last_p = 0
            while True:
                now = time.time()
                if now - last_p > 0.08:
                    s.write(b"UXTDWU")
                    last_p = now
                data = s.read(64)
                if data:
                    print(f"\n[!!!] ОТВЕТ ОТ ЧИПА: {data}")
                    print(f"      HEX: {' '.join(f'{b:02X}' for b in data)}")
                    if b"cmd>>:" in data:
                        print("\n[+++] БИНГО! ЧИП В БУТЛОАДЕРЕ (cmd>>:)! МОЖНО СНИМАТЬ ДАМП!")
                        break
                time.sleep(0.01)
    except KeyboardInterrupt:
        print("\nОстановлено.")
    except Exception as e:
        print(f"[!] Ошибка: {e}")
