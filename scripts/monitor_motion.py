import serial
import time
import sys

def main():
    s = serial.Serial('COM5', 115200, timeout=0.1)
    time.sleep(0.1)
    s.reset_input_buffer()
    
    print("=" * 60)
    print("[*] LIVE MOTION MONITOR (HK01BS on GPIO_P18)")
    print("[*] Monitoring pulses and steps... (Press Ctrl+C to stop)")
    print("[*] Try tapping or tilting or shaking the watch!")
    print("=" * 60)
    
    last_val = None
    t0 = time.time()
    
    try:
        while time.time() - t0 < 8.0: # Run for 8 seconds
            s.write(b"motion\r\n")
            time.sleep(0.1)
            raw = s.read(s.in_waiting or 1).decode('latin1', errors='replace')
            for line in raw.splitlines():
                if "Motion (HK01BS):" in line:
                    if line != last_val:
                        print(f"[{time.strftime('%H:%M:%S')}] {line.strip()}")
                        last_val = line
    except KeyboardInterrupt:
        pass
    finally:
        s.close()

if __name__ == "__main__":
    main()
