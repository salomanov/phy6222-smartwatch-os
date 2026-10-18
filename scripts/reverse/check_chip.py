import sys
import os
import subprocess
import serial.tools.list_ports

def find_serial_port():
    ports = list(serial.tools.list_ports.comports())
    # Ищем USB-UART адаптеры (CH340, CP210x, FTDI, Prolific и т.д.)
    usb_ports = [p for p in ports if "BTHENUM" not in p.hwid and ("USB" in p.hwid or "VID" in p.hwid)]
    if usb_ports:
        return usb_ports[0].device
    if ports:
        return ports[0].device
    return None

def main():
    port = sys.argv[1] if len(sys.argv) > 1 else find_serial_port()
    if not port:
        print("[!] ОШИБКА: USB-UART адаптер не обнаружен!")
        print("    Подключите переходник (CH340/CP2102) к USB и повторите запуск.")
        sys.exit(1)
        
    print(f"[*] Используем порт: {port}")
    print("[*] Проверка связи с чипом PHY6212 (режим TM = 1)...")
    print("    Если связи нет, убедитесь, что пятак TM замкнут на 3.3V, и кратковременно замкните EN на GND.\n")
    
    script_path = os.path.join(os.path.dirname(__file__), "tools", "rdwr_phy62x2.py")
    cmd = [sys.executable, script_path, "-p", port, "-b", "115200", "-t", "i"]
    res = subprocess.run(cmd)
    sys.exit(res.returncode)

if __name__ == "__main__":
    main()
