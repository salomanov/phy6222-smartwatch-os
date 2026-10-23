import serial
import time

def read_line(ser, timeout=0.1):
    t0 = time.time()
    buf = bytearray()
    while time.time() - t0 < timeout:
        if ser.in_waiting:
            c = ser.read(1)
            buf += c
            if c == b'\n':
                break
        else:
            time.sleep(0.002)
    return buf.decode('latin1', errors='replace')

def cmd(ser, command, timeout=0.15):
    ser.reset_input_buffer()
    ser.write(command.encode() + b'\r\n')
    t0 = time.time()
    resp = ""
    while time.time() - t0 < timeout:
        chunk = ser.read(ser.in_waiting or 1).decode('latin1', errors='replace')
        resp += chunk
        if "cmd>>" in resp:
            break
    return resp

def run_test():
    ser = serial.Serial('COM5', 115200, timeout=0.05)
    time.sleep(0.1)

    print("[*] Baseline check for P18:")
    for _ in range(5):
        r = cmd(ser, "read 18")
        for line in r.splitlines():
            if "Pin P18" in line:
                print("   ", line.strip())

    print("\n[*] Triggering vibration (vib 150) and reading P18 as fast as possible:")
    ser.reset_input_buffer()
    ser.write(b"vib 150\r\n")
    
    t0 = time.time()
    samples = []
    while time.time() - t0 < 0.35:
        t_req = (time.time() - t0) * 1000
        ser.write(b"read 18\r\n")
        # Read until response
        r = ""
        tr0 = time.time()
        while time.time() - tr0 < 0.05:
            c = ser.read(ser.in_waiting or 1).decode('latin1', errors='replace')
            r += c
            if "cmd>>" in r:
                break
        val = None
        for line in r.splitlines():
            if "Pin P18 =" in line:
                val = line.split("=")[1].strip()
        if val is not None:
            samples.append((round(t_req, 1), val))

    print(f"\n[*] Total samples during/after vibration: {len(samples)}")
    for t_ms, val in samples:
        print(f"  +{t_ms:5.1f} ms -> P18 = {val}")

    ones = [s for s in samples if s[1] == '1']
    if ones:
        print(f"\n[!!!] DETECTED {len(ones)} TRANSITION(S) TO HIGH (1)!")
    else:
        print("\n[*] All samples remained 0.")

    ser.close()

if __name__ == "__main__":
    run_test()
