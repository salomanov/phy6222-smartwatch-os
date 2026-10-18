import serial
import serial.tools.list_ports
import time
import sys

def get_com_port():
    for p in serial.tools.list_ports.comports():
        if "CH34" in p.description or "USB-SERIAL" in p.description or "COM5" in p.device:
            return p.device
    return "COM5"

def main():
    port = sys.argv[1] if len(sys.argv) > 1 else get_com_port()
    baud = 115200
    print("=" * 65)
    print(f"[*] СЛУШАЕМ UART ЧАСОВ НА {port} ({baud} бод)")
    print("=" * 65)
    print("[*] Перезагрузите часы (кратковременно сбросьте питание или EN на GND)")
    print("[*] Либо нажмите Connect в nRF Connect / браузере...")
    print("[*] Для выхода нажмите Ctrl+C\n")

    try:
        ser = serial.Serial(port, baud, timeout=0.1)
    except Exception as e:
        print(f"[!] Ошибка открытия {port}: {e}")
        return

    buf = bytearray()
    while True:
        try:
            raw = ser.read(ser.in_waiting or 1)
            if raw:
                sys.stdout.buffer.write(raw)
                sys.stdout.buffer.flush()
        except KeyboardInterrupt:
            break
        except Exception as e:
            print(f"\n[!] Ошибка: {e}")
            break

    ser.close()
    print("\n[*] Завершено.")

if __name__ == "__main__":
    main()
