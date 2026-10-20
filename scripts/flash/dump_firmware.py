import sys
import os
import time
import subprocess
import serial.tools.list_ports

def find_serial_port():
    ports = list(serial.tools.list_ports.comports())
    usb_ports = [p for p in ports if "BTHENUM" not in p.hwid and ("USB" in p.hwid or "VID" in p.hwid)]
    if usb_ports:
        return usb_ports[0].device
    if ports:
        return ports[0].device
    return "COM5"

def main():
    port = sys.argv[1] if len(sys.argv) > 1 else find_serial_port()
    baud = sys.argv[2] if len(sys.argv) > 2 else "115200"
    
    ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    backup_dir = os.path.join(ROOT_DIR, ".backups")
    os.makedirs(backup_dir, exist_ok=True)
    
    timestamp = time.strftime("%Y%m%d_%H%M%S")
    dump_filename = f"factory_dump_{timestamp}.bin"
    dump_path = os.path.join(backup_dir, dump_filename)
    latest_dump_path = os.path.join(backup_dir, "factory_dump.bin")
    
    print("=" * 68)
    print(f"[*] СНЯТИЕ ПОЛНОГО ДАМПА FLASH (512 КБ) C CHIP PHY6222")
    print("=" * 68)
    print(f"[*] Порт: {port}")
    print(f"[*] Скорость: {baud} бод")
    print(f"[*] Файл: {dump_path}")
    print("-" * 68)
    print("[!] Если программа запросит 'Release RST_N / Turn on power':")
    print("    Сделайте сброс тем же способом (передёрните питание или чиркните ножку сброса)!")
    print("=" * 68 + "\n")
    
    script_path = os.path.join(ROOT_DIR, "tools", "rdwr_phy62x2.py")
    cmd = [sys.executable, script_path, "-p", port, "-b", baud, "-t", "rf", dump_path]
    res = subprocess.run(cmd)
    
    if res.returncode == 0 and os.path.exists(dump_path) and os.path.getsize(dump_path) > 0:
        size = os.path.getsize(dump_path)
        # Копируем также как factory_dump.bin
        with open(dump_path, "rb") as src, open(latest_dump_path, "wb") as dst:
            dst.write(src.read())
        print("\n" + "=" * 68)
        print(f"[+++] УСПЕХ! ЗАВОДСКОЙ ДАМП ПОЛНОСТЬЮ СНЯТ И СОХРАНЁН!")
        print(f"      Размер: {size} байт ({size // 1024} КБ)")
        print(f"      Файл 1: {dump_path}")
        print(f"      Файл 2: {latest_dump_path}")
        print("=" * 68)
    else:
        print("\n[-] Ошибка снятия дампа. Повторите запуск.")
        
    sys.exit(res.returncode)

if __name__ == "__main__":
    main()
