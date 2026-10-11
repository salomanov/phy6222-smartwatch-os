import serial
import time
import sys

LOG_FILE = "uart_live.log"

def main():
    port = sys.argv[1] if len(sys.argv) > 1 else "COM5"
    baud = 115200
    print(f"[*] Открытие UART монитора на {port} ({baud} бод)...")
    try:
        ser = serial.Serial(port, baud, timeout=0.1)
    except Exception as e:
        print(f"[!] Не удалось открыть {port}: {e}")
        return

    print(f"[+] {port} открыт. Запись лога в {LOG_FILE}...")
    with open(LOG_FILE, "a", encoding="utf-8", errors="replace") as f:
        f.write(f"\n--- UART LOG SESSION START: {time.ctime()} ---\n")
        f.flush()
        while True:
            try:
                data = ser.read(ser.in_waiting or 1)
                if data:
                    text = data.decode("utf-8", errors="replace")
                    sys.stdout.write(text)
                    sys.stdout.flush()
                    f.write(text)
                    f.flush()
            except Exception as e:
                print(f"[!] Ошибка чтения UART: {e}")
                break

if __name__ == "__main__":
    main()
