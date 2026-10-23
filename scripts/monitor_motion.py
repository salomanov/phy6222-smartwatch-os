import serial
import time
import sys

def main():
    print("=" * 65)
    print("[*] LIVE MOTION MONITOR (HK01BS on GPIO_P18) — ТЕСТ 60 СЕКУНД")
    print("[*] ПОСТУЧИТЕ ПАЛЬЦЕМ, ВСТРЯХНИТЕ, ПОВЕРНИТЕ ЧАСЫ В РАЗНЫЕ СТОРОНЫ!")
    print("=" * 65)
    
    try:
        s = serial.Serial('COM5', 115200, timeout=0.1)
    except Exception as e:
        print(f"[!] Ошибка открытия COM5: {e}")
        return

    time.sleep(0.1)
    s.reset_input_buffer()
    
    # Initial status
    s.write(b"motion\r\n")
    time.sleep(0.15)
    init_out = s.read(s.in_waiting or 100).decode('latin1', errors='replace')
    for line in init_out.splitlines():
        if "Motion (HK01BS):" in line:
            print(f"[*] Старт: {line.strip()}")

    t0 = time.time()
    duration = 60.0
    last_printed = ""
    last_heartbeat = t0
    events_caught = 0

    try:
        while time.time() - t0 < duration:
            s.write(b"motion\r\n")
            time.sleep(0.12)
            raw = s.read(s.in_waiting or 100).decode('latin1', errors='replace')
            for line in raw.splitlines():
                if "Motion (HK01BS):" in line:
                    clean = line.strip()
                    if clean != last_printed:
                        rem = duration - (time.time() - t0)
                        print(f"[{time.strftime('%H:%M:%S')}] (+{time.time()-t0:4.1f}с, ост. {rem:4.1f}с) -> {clean}")
                        last_printed = clean
                        events_caught += 1
            
            # Heartbeat every 10 seconds if no changes
            now = time.time()
            if now - last_heartbeat >= 10.0:
                last_heartbeat = now
                rem = duration - (now - t0)
                print(f"  ...мониторинг идёт ({int(now-t0)}с / {int(duration)}с)...")

    except KeyboardInterrupt:
        print("\n[*] Прервано пользователем.")
    finally:
        print("\n" + "=" * 65)
        # Final read
        s.write(b"motion\r\n")
        time.sleep(0.15)
        fin_out = s.read(s.in_waiting or 100).decode('latin1', errors='replace')
        for line in fin_out.splitlines():
            if "Motion (HK01BS):" in line:
                print(f"[*] Итог замера (60 сек): {line.strip()}")
        s.close()
        print("=" * 65)

if __name__ == "__main__":
    main()
