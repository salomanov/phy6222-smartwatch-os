import sys
import os
import time
import subprocess
import datetime
import serial.tools.list_ports

def find_serial_port():
    print("[*] Поиск USB-UART адаптера...")
    last_print = 0
    while True:
        ports = list(serial.tools.list_ports.comports())
        usb_ports = [p for p in ports if "BTHENUM" not in p.hwid and ("USB" in p.hwid or "VID" in p.hwid or "CH34" in p.description or "CP210" in p.description or "FTDI" in p.description or "Serial" in p.description)]
        if usb_ports:
            print(f"[+] Найден адаптер: {usb_ports[0].device} ({usb_ports[0].description})")
            return usb_ports[0].device
        non_bt = [p for p in ports if "BTHENUM" not in p.hwid and "Bluetooth" not in p.description]
        if non_bt:
            print(f"[+] Найден порт: {non_bt[0].device} ({non_bt[0].description})")
            return non_bt[0].device
            
        cur = time.time()
        if cur - last_print > 3:
            print("[...] Ожидание подключения USB-UART адаптера к ПК...")
            last_print = cur
        time.sleep(0.5)

def main():
    port = sys.argv[1] if len(sys.argv) > 1 else find_serial_port()
    baud = sys.argv[2] if len(sys.argv) > 2 else "115200"
    
    dumps_dir = os.path.join(os.path.dirname(__file__), "dumps")
    os.makedirs(dumps_dir, exist_ok=True)
    
    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    out_file = os.path.join(dumps_dir, f"flash_dump_{timestamp}.bin")
    
    print("=" * 70)
    print(f"[*] СЧИТЫВАНИЕ ТЕКУЩЕЙ FLASH ПАМЯТИ ЧАСОВ (PHY6222)")
    print("=" * 70)
    print(f"[*] Порт:      {port}")
    print(f"[*] Скорость:  {baud} бод")
    print(f"[*] Куда:      {out_file}")
    print("=" * 70)
    print("[*] СХЕМА ПОДКЛЮЧЕНИЯ:")
    print("    - GND адаптера -> пятак GND")
    print("    - TX адаптера  -> пятак RX (ножка 9)")
    print("    - RX адаптера  -> пятак TX (ножка 8)")
    print("    - TM часов     -> 3.3V (подключен постоянно)")
    print("    - VCC3V        -> провод 3.3V держим в руке, ПОКА НЕ ПОДКЛЮЧАЕМ")
    print("=" * 70)
    print("[!] ИНСТРУКЦИЯ:")
    print("    Сейчас начнётся опрос чипа...")
    print("    КАК ТОЛЬКО появится строка 'Turn on the power' / точки '......':")
    print("    ПОДКЛЮЧИТЕ провод 3.3V к пятаку VCC3V (холодный старт чипа)")
    print("    (или кратковременно замкните контакт RESET на GND)!")
    print("=" * 70 + "\n")
    
    time.sleep(1)
    
    script_path = os.path.join(os.path.dirname(__file__), "tools", "rdwr_phy62x2.py")
    # rf = Read all Flash (512 KB)
    cmd = [sys.executable, "-u", script_path, "-p", port, "-b", baud, "-t", "rf", out_file]
    res = subprocess.run(cmd)
    
    if res.returncode == 0 and os.path.exists(out_file) and os.path.getsize(out_file) > 0:
        size = os.path.getsize(out_file)
        print("\n" + "=" * 70)
        print(f"[+++] ДАМП FLASH ПАМЯТИ УСПЕШНО СЧИТАН ({size} байт)!")
        print(f"[+] Сохранён в: {out_file}")
        print("=" * 70)
    else:
        print("\n[-] Ошибка считывания. Проверьте контакты RX/TX, TM и питание, затем повторите.")
        
    sys.exit(res.returncode)

if __name__ == "__main__":
    main()
