import serial
import time
import sys

port = 'COM5'
print(f"[*] Запуск сессии прослушивания на 10 секунд (порт {port}, 115200)...")
print("[*] ВНИМАНИЕ: сейчас сделайте сброс EN на GND (коснитесь и отпустите)!\n")

try:
    with serial.Serial(port, 115200, timeout=0.05) as s:
        s.reset_input_buffer()
        s.reset_output_buffer()
        
        t_end = time.time() + 10.0
        last_ping = 0
        total_bytes = 0
        
        while time.time() < t_end:
            now = time.time()
            if now - last_ping > 0.15:
                s.write(b"UXTDWU")
                last_ping = now
                
            rem = int(t_end - now)
            data = s.read(128)
            if data:
                total_bytes += len(data)
                print(f"\n[+] НАЙДЕНЫ ДАННЫЕ! Получено {len(data)} байт: {data}")
                print(f"    HEX: {' '.join(f'{b:02X}' for b in data)}")
            time.sleep(0.02)
            
        print("\n" + "=" * 50)
        if total_bytes == 0:
            print("[-] За 10 секунд принято 0 байт (полная тишина).")
        else:
            print(f"[+] Всего принято: {total_bytes} байт.")
        print("=" * 50)
except Exception as e:
    print(f"[!] Ошибка порта: {e}")
