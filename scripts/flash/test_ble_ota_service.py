import asyncio
from bleak import BleakScanner, BleakClient

OTA_SERVICE_UUID = "5833ff01-9b8b-5191-6142-22a4536ef123"

async def test_ota_service():
    print("[*] Поиск CHASY-STEALTH...")
    device = await BleakScanner.find_device_by_name("CHASY-STEALTH", timeout=7.0)
    if not device:
        print("[!] Устройство не найдено.")
        return

    print(f"[+] Найдено: {device.name} [{device.address}]. Подключение с фильтром сервиса...")
    # Ограничиваем запрос GATT ТОЛЬКО OTA-сервисом!
    async with BleakClient(device, services=[OTA_SERVICE_UUID], timeout=15.0) as client:
        print(f"[+++] УСПЕШНО ПОДКЛЮЧЕНО К ЧАСАМ! is_connected = {client.is_connected}")
        ota_svc = client.services.get_service(OTA_SERVICE_UUID)
        print(f"[+] Найден OTA сервис: {ota_svc}")
        for c in ota_svc.characteristics:
            print(f"    Char: {c.uuid} ({','.join(c.properties)})")

if __name__ == "__main__":
    asyncio.run(test_ota_service())
