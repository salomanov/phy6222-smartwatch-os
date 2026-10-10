import sys
import os
import time
import subprocess
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
        # If no explicit USB description matched, check if any non-bluetooth port appeared
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
    
    hex_path = os.path.join(os.path.dirname(__file__), "firmware", "build", "custom_firmware.hex")
    if not os.path.exists(hex_path):
        print(f"[!] Файл прошивки не найден: {hex_path}")
        print("[!] Сначала соберите проект через 'python firmware/build.py'")
        sys.exit(1)
        
    print("=" * 70)
    print(f"[*] ЗАПИСЬ КАСТОМНОЙ ПРОШИВКИ СМАРТ-ЧАСОВ (PHY6222)")
    print("=" * 70)
    print(f"[*] Порт:      {port}")
    print(f"[*] Скорость:  {baud} бод")
    print(f"[*] Файл:      {hex_path} ({os.path.getsize(hex_path)} байт)")
    print("=" * 70)
    print("[*] СХЕМА ПОДКЛЮЧЕНИЯ:")
    print("    - GND адаптера -> пятак GND")
    print("    - TX адаптера  -> пятак RX (ножка 9)")
    print("    - RX адаптера  -> пятак TX (ножка 8)")
    print("    - TM часов     -> 3.3V (подключен постоянно!)")
    print("    - VCC3V        -> провод 3.3V держим в руке, ПОКА НЕ ПОДКЛЮЧАЕМ")
    print("=" * 70)
    print("[!] ИНСТРУКЦИЯ:")
    print("    Сейчас начнётся опрос чипа...")
    print("    КАК ТОЛЬКО появится надпись 'Turn on the power' / 'Release RST_N':")
    print("    ПОДКЛЮЧИТЕ провод 3.3V к пятаку VCC3V (холодный старт чипа)!")
    print("=" * 70 + "\n")
    
    time.sleep(1)
    
    script_path = os.path.join(os.path.dirname(__file__), "tools", "rdwr_phy62x2.py")
    cmd = [sys.executable, script_path, "-p", port, "-b", baud, "-t", "wh", hex_path]
    res = subprocess.run(cmd)
    
    if res.returncode == 0:
        print("\n" + "=" * 70)
        print("[+++] ПРОШИВКА УСПЕШНО ЗАПИСАНА ВО FLASH ПАМЯТЬ ЧАСОВ!")
        print("=" * 70)
        print("[*] ЧТО ДЕЛАТЬ ДАЛЬШЕ ДЛЯ ЗАПУСКА ЧАСОВ В ОБЫЧНОМ РЕЖИМЕ:")
        print("    1. Отключите провод от пятака TM (чтобы выйти из Bootloader)")
        print("    2. Отключите и снова подключите питание 3.3V на VCC3V (перезагрузка)")
        print("    3. Экран часов загорится и отобразит Watchface с часами и шпорой!")
        print("=" * 70)
    else:
        print("\n[-] Ошибка записи прошивки. Проверьте контакты и повторите попытку.")
        
    sys.exit(res.returncode)

if __name__ == "__main__":
    main()
