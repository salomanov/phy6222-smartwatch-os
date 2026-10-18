import asyncio
from bleak import BleakScanner, BleakClient

async def test_conn():
    print("[*] Поиск устройства CHASY-STEALTH...")
    device = await BleakScanner.find_device_by_name("CHASY-STEALTH", timeout=10.0)
    if not device:
        print("[!] Устройство CHASY-STEALTH не найдено в эфире! Пробуем по адресу...")
        device = await BleakScanner.find_device_by_address("62:62:15:07:07:15", timeout=5.0)
        
    if not device:
        print("[!] Не удалось обнаружить часы.")
        return

    print(f"[+] Устройство обнаружено: {device.name} ({device.address}). Подключаемся...")
    async with BleakClient(device, timeout=20.0) as client:
        print(f"[+++] УСПЕШНО ПОДКЛЮЧЕНО! is_connected = {client.is_connected}")
        print("[*] Чтение сервисов:")
        for s in client.services:
            print(f"  Service: {s.uuid}")
            for c in s.characteristics:
                print(f"    Char: {c.uuid} ({','.join(c.properties)})")

if __name__ == "__main__":
    asyncio.run(test_conn())
