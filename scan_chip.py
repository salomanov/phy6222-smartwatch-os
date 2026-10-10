import serial
import serial.tools.list_ports
import time
import sys
import subprocess

def find_port():
    if len(sys.argv) > 1:
        return sys.argv[1]
    ports = list(serial.tools.list_ports.comports())
    for p in ports:
        if "CH34" in p.description or "USB" in p.hwid or "VID" in p.hwid:
            if "BTHENUM" not in p.hwid:
                return p.device
    return "COM5"

port = find_port()
print("=" * 65)
print(f"[*] СКАНИРОВАНИЕ И ОПРОС ЧИПА PHY6212 НА ПОРТУ {port} (115200)")
print("=" * 65)
print("[*] Я запустил непрерывный опрос чипа на 60 секунд.")
print("[*] СЕЙЧАС КРАТКОВРЕМЕННО КОСНИТЕСЬ ПИНА EN К МАССЕ (GND) И ОТПУСТИТЕ!")
print("[*] Если чип ответит 'cmd>>:', мы сразу зафиксируем связь.")
print("=" * 65)

duration = 60
t_start = time.time()
t_end = t_start + duration

try:
    with serial.Serial(port, 115200, timeout=0.04) as s:
        s.reset_input_buffer()
        s.reset_output_buffer()
        
        last_ping = 0
        last_show = 0
        total_rx = 0
        found_sync = False
        
        while time.time() < t_end:
            now = time.time()
            rem = int(t_end - now)
            
            # Посылаем sync каждые 80 мс
            if now - last_ping > 0.08:
                s.write(b"UXTDWU")
                last_ping = now
                
            # Читаем всё входящее
            incoming = s.read(128)
            if incoming:
                total_rx += len(incoming)
                hex_vals = " ".join(f"{b:02X}" for b in incoming)
                print(f"\n[!!!] ПОЛУЧЕН ОТВЕТ ({len(incoming)} байт): {incoming}")
                print(f"      HEX: {hex_vals}")
                if b"cmd>>:" in incoming:
                    print("\n[+] УСПЕХ! Чип перешёл в режим загрузчика (cmd>>:)!")
                    found_sync = True
                    break
                elif b"fct>>:" in incoming:
                    print("\n[+] ЧИП В FCT РЕЖИМЕ (fct>>:)!")
                    found_sync = True
                    break
            
            # Печатаем обратный отсчёт каждую секунду
            if now - last_show >= 1.0:
                sys.stdout.write(f"\r[*] Опрос активен... Осталось {rem:2d} сек | Принято байт: {total_rx} | Ждём касания EN...")
                sys.stdout.flush()
                last_show = now
                
            time.sleep(0.01)
            
        print("\n")
        if found_sync:
            print("=" * 65)
            print("[+] СВЯЗЬ УСТАНОВЛЕНА! ЧИП ОТКЛИКАЕТСЯ!")
            print("=" * 65)
            s.close()
            # Попробуем прочитать ревизию через rdwr_phy62x2
            print("[*] Запускаем чтение ревизии...")
            subprocess.run([sys.executable, "tools/rdwr_phy62x2.py", "-p", port, "check"])
        else:
            print("=" * 65)
            if total_rx == 0:
                print("[-] Время вышло. 0 байт получено.")
                print("[-] Внимание: проверьте подключение RX/TX и второй золотистый пятак!")
            else:
                print(f"[?] Получено {total_rx} байт, но метка 'cmd>>:' не распознана.")
            print("=" * 65)

except Exception as e:
    print(f"\n[!] Ошибка работы с портом {port}: {e}")
