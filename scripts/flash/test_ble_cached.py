import asyncio
from bleak import BleakScanner, BleakClient

async def test_cached():
    print("[*] Поиск CHASY-STEALTH...")
    device = await BleakScanner.find_device_by_name("CHASY-STEALTH", timeout=7.0)
    if not device:
        print("[!] Не найдено.")
        return

    print(f"[+] Найдено: {device.name} [{device.address}]. Подключение с winrt cached...")
    try:
        # В Bleak для WinRT есть backend опция use_cached_services
        client = BleakClient(device, timeout=10.0, winrt={"use_cached_services": True})
        await client.connect()
        print(f"[+++] ПОДКЛЮЧЕНО! is_connected = {client.is_connected}")
        for s in client.services:
            print(f"Service: {s.uuid}")
        await client.disconnect()
    except Exception as e:
        print("[!] Ошибка:", e)

if __name__ == "__main__":
    asyncio.run(test_cached())
