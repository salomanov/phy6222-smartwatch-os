import asyncio
from bleak import BleakScanner, BleakClient

async def test_pair():
    print("[*] Поиск CHASY-STEALTH...")
    device = await BleakScanner.find_device_by_name("CHASY-STEALTH", timeout=7.0)
    if not device:
        print("[!] Не найдено.")
        return

    print(f"[+] Найдено: {device.name} [{device.address}]. Подключение с pair=True...")
    try:
        async with BleakClient(device, pair=True, timeout=20.0) as client:
            print(f"[+++] ПОДКЛЮЧЕНО! is_connected = {client.is_connected}")
            for s in client.services:
                print(f"Service: {s.uuid}")
    except Exception as e:
        print("[!] Ошибка:", e)

if __name__ == "__main__":
    asyncio.run(test_pair())
