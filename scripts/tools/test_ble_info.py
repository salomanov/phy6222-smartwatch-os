import asyncio
import sys
from bleak import BleakScanner, BleakClient

DEVINFO_FW_REV = "00002a26-0000-1000-8000-00805f9b34fb"
DEVINFO_MODEL  = "00002a24-0000-1000-8000-00805f9b34fb"
DEVINFO_MANU   = "00002a29-0000-1000-8000-00805f9b34fb"

async def main():
    print("[*] Scanning for CHASY-STEALTH...", flush=True)
    target_addr = "62:62:15:07:07:15"
    found_dev = [None]
    found_evt = asyncio.Event()

    def cb(dev, adv):
        if dev.address.upper() == target_addr.upper() or (dev.name and "CHASY" in dev.name.upper()):
            print(f"[+] Discovered: {dev.name} ({dev.address}), RSSI: {adv.rssi}", flush=True)
            found_dev[0] = dev
            found_evt.set()

    scanner = BleakScanner(detection_callback=cb)
    await scanner.start()
    try:
        await asyncio.wait_for(found_evt.wait(), timeout=12.0)
    except asyncio.TimeoutError:
        print("[!] Timeout waiting for advertisement", flush=True)
        return
    finally:
        await scanner.stop()

    client = None
    for attempt in range(1, 4):
        try:
            print(f"[*] Connecting (attempt {attempt}/3)...", flush=True)
            client = BleakClient(found_dev[0], timeout=20.0)
            await client.connect()
            print("[+] Connected successfully!", flush=True)
            break
        except Exception as e:
            print(f"[!] Attempt {attempt} failed: {e}", flush=True)
            await asyncio.sleep(2.0)

    if not client or not client.is_connected:
        print("[!] Could not connect after 3 attempts.", flush=True)
        return

    try:
        print(f"[+] MTU = {client.mtu_size}", flush=True)
        for char_uuid, label in [(DEVINFO_FW_REV, "Firmware Rev"),
                                 (DEVINFO_MODEL,  "Model"),
                                 (DEVINFO_MANU,   "Manufacturer")]:
            try:
                val = await client.read_gatt_char(char_uuid)
                print(f"  [+] {label}: {val.decode('utf-8', errors='replace')} (raw: {val})", flush=True)
            except Exception as e:
                print(f"  [!] Could not read {label}: {e}", flush=True)
    finally:
        await client.disconnect()
        print("[+] Disconnected cleanly.", flush=True)

if __name__ == '__main__':
    asyncio.run(main())
