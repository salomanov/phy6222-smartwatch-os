import asyncio
import datetime
import sys
from bleak import BleakClient, BleakScanner

CHAR1_UUID = "0000fff1-0000-1000-8000-00805f9b34fb"
TARGET_ADDR = "62:62:15:07:07:15"

async def main():
    print(f"[*] BLE Clock Synchronizer for LP715 (CHASY-STEALTH)")
    now = datetime.datetime.now()
    time_str = f"TIME:{now.hour:02d}:{now.minute:02d}:{now.second:02d}"
    # Characteristic 1 requires exactly 16 bytes (IBEACON_UUID_LEN)
    payload = time_str.encode("ascii").ljust(16, b'\x00')
    print(f"[*] Local PC Time: {now.strftime('%H:%M:%S')}")
    print(f"[*] Payload (16 bytes): {payload} (hex: {payload.hex()})")

    print(f"[*] Connecting to {TARGET_ADDR}...")
    try:
        async with BleakClient(TARGET_ADDR, timeout=12.0) as client:
            if not client.is_connected:
                print(f"[!] Failed to connect to {TARGET_ADDR}")
                return 1
            print(f"[+] Connected to {TARGET_ADDR}!")
            
            # Read current Char1 value before writing
            val_before = await client.read_gatt_char(CHAR1_UUID)
            print(f"[*] Current Char1 value: {val_before.hex()}")

            # Write the synchronized time payload
            print(f"[*] Writing time sync packet...")
            await client.write_gatt_char(CHAR1_UUID, payload, response=True)
            print(f"[+] Time sync packet sent successfully!")

            # Read back
            val_after = await client.read_gatt_char(CHAR1_UUID)
            print(f"[*] Verified Char1 readback: {val_after.hex()} ('{val_after[:13].decode('latin1', errors='ignore')}')")
            print(f"[+] Watch time synchronized to {now.strftime('%H:%M:%S')}! Haptic pulse triggered.")
            return 0
    except Exception as e:
        print(f"[!] Error: {e}")
        return 2

if __name__ == "__main__":
    ret = asyncio.run(main())
    sys.exit(ret)
