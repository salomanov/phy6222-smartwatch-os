import serial
import time
import sys

def main():
    print("==================================================")
    print("      CHASY-STEALTH: HR / LED PIN TESTER          ")
    print("==================================================")
    print("Safe pins for optical sensor testing:")
    print("  P00 - Optical sensor power switch candidate")
    print("  P14 - Green LED candidate (Analog_IO_1)")
    print("  P02 - Spare GPIO / Analog_IO")
    print("  P23 - Analog_IO_7 / HR signal candidate")
    print("  P15 - Main sensors power enable")
    print("--------------------------------------------------")

    try:
        ser = serial.Serial('COM5', 115200, timeout=1.0)
    except Exception as e:
        print(f"[!] Cannot open COM5: {e}")
        return

    # Check connection
    ser.write(b'\r\nping\r\n')
    time.sleep(0.2)
    resp = ser.read_all().decode('ascii', errors='ignore')
    if 'pong' not in resp:
        print("[!] Warning: watch did not respond to ping on COM5.")
    else:
        print("[+] Watch connected and responding on COM5!\n")

    pin_states = {0: 1, 14: 0, 2: 0, 23: 0, 15: 1}

    def send_cmd(cmd_str):
        ser.write((cmd_str + '\r\n').encode('ascii'))
        time.sleep(0.15)
        out = ser.read_all().decode('ascii', errors='ignore').strip()
        lines = [l for l in out.split('\n') if 'cmd>>' not in l and l.strip()]
        return ' | '.join(lines) if lines else 'OK'

    while True:
        print("\n--- Current Pin States ---")
        for p in sorted(pin_states.keys()):
            val = pin_states[p]
            desc = {0: "HR PWR", 14: "LED Cand", 2: "Spare", 23: "AIO7/HR", 15: "Sens PWR"}.get(p, "")
            print(f"  P{p:02d} ({desc:9s}): {val}  ({'HIGH' if val else 'LOW '})")
        print("---------------------------")
        print("Commands:")
        print("  0       - Toggle P00 (HR Power switch)")
        print("  14      - Toggle P14 (Green LED candidate 1)")
        print("  2       - Toggle P02 (Candidate 2)")
        print("  23      - Toggle P23 (Candidate 3)")
        print("  15      - Toggle P15 (Sensor Power)")
        print("  sweep   - Sweep pulse (2s each) on all candidates")
        print("  inv     - Invert all active pins")
        print("  off     - Turn everything OFF (P00=1, all others=0)")
        print("  q       - Exit")
        
        try:
            choice = input("\nSelect command: ").strip().lower()
        except (KeyboardInterrupt, EOFError):
            break

        if choice in ('q', 'exit', 'quit'):
            break
        elif choice == 'off':
            print("[*] Setting safe OFF state...")
            send_cmd("pin 0 1")
            send_cmd("pin 14 0")
            send_cmd("pin 2 0")
            send_cmd("pin 23 0")
            pin_states[0] = 1
            pin_states[14] = 0
            pin_states[2] = 0
            pin_states[23] = 0
            print("[+] All candidates OFF.")
        elif choice == 'inv':
            print("[*] Inverting all candidate states...")
            for p in [0, 14, 2, 23]:
                new_v = 1 if pin_states[p] == 0 else 0
                r = send_cmd(f"pin {p} {new_v}")
                pin_states[p] = new_v
                print(f"  P{p:02d} -> {new_v} ({r})")
        elif choice == 'sweep':
            print("[*] Starting sweep pulse across candidate pins...")
            print("[*] Testing combinations of P00 (PWR) and LED candidates...")
            for pwr in [0, 1]:
                print(f"\n--- Testing with P00 = {pwr} (Power {'ON' if pwr==0 else 'OFF'}) ---")
                send_cmd(f"pin 0 {pwr}")
                pin_states[0] = pwr
                for led_pin in [14, 2, 23]:
                    for pol in [1, 0]:
                        print(f"  -> Testing P{led_pin:02d} = {pol} (Look at back cover now!)...")
                        send_cmd(f"pin {led_pin} {pol}")
                        time.sleep(2.0)
                        send_cmd(f"pin {led_pin} {1 - pol}")
            send_cmd("pin 0 1")
            send_cmd("pin 14 0")
            send_cmd("pin 2 0")
            send_cmd("pin 23 0")
            pin_states[0] = 1
            pin_states[14] = 0
            pin_states[2] = 0
            pin_states[23] = 0
            print("\n[+] Sweep complete. All returned to safe idle.")
        elif choice in ('0', '14', '2', '23', '15'):
            p = int(choice)
            new_v = 1 if pin_states[p] == 0 else 0
            r = send_cmd(f"pin {p} {new_v}")
            pin_states[p] = new_v
            print(f"[OK] P{p:02d} set to {new_v}: {r}")
        else:
            print("[!] Unknown command. Type 0, 14, 2, 23, sweep, off, or q.")

    ser.close()
    print("\n[+] Serial port closed. Goodbye!")

if __name__ == '__main__':
    main()
