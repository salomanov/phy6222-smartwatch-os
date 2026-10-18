import asyncio
from bleak import BleakClient, BleakScanner

CHAR6_UUID = "0000fff6-0000-1000-8000-00805f9b34fb"
CHAR4_UUID = "0000fff4-0000-1000-8000-00805f9b34fb"
CHAR1_UUID = "0000fff1-0000-1000-8000-00805f9b34fb"

def notif_handler(sender, data):
    try:
        text = data.decode('utf-8', errors='replace').strip('\x00')
        print(f"  [BLE NOTIF] from {sender}: {text}")
    except Exception as e:
        print(f"  [BLE NOTIF raw] from {sender}: {data.hex()}")

async def main():
    print("[*] Finding CHASY-STEALTH...")
    device = await BleakScanner.find_device_by_filter(
        lambda d, adv: d.address.upper() == '62:62:15:07:07:15' or (d.name and 'CHASY' in d.name),
        timeout=5.0
    )
    if not device:
        print("[!] Device not found.")
        return

    print(f"[+] Connecting to {device.name} ({device.address})...")
    async with BleakClient(device, timeout=15.0) as client:
        print("[+] Connected!")
        print("[*] Enumerating services:")
        for s in client.services:
            print(f"  Service: {s.uuid} ({s.description})")
            for ch in s.characteristics:
                props = ",".join(ch.properties)
                print(f"    Char: {ch.uuid} [{props}]")

        # Try reading CHAR6
        try:
            val = await client.read_gatt_char(CHAR6_UUID)
            txt = val.decode('utf-8', errors='replace').strip('\x00')
            print(f"\n[+] Direct READ of CHAR6 (0xFFF6): '{txt}' (raw: {val.hex()})")
        except Exception as e:
            print(f"[!] Read CHAR6 error: {e}")

        # Subscribe to notifications on CHAR6
        try:
            print("[*] Subscribing to CHAR6 notifications for 8 seconds...")
            await client.start_notify(CHAR6_UUID, notif_handler)
            await asyncio.sleep(8.0)
            await client.stop_notify(CHAR6_UUID)
        except Exception as e:
            print(f"[!] Notify CHAR6 error: {e}")

asyncio.run(main())
