import asyncio
import sys
import uuid
from bleak import BleakScanner
from bleak.backends.winrt.client import FutureLike
from winrt.windows.devices.bluetooth import BluetoothLEDevice
import winrt.windows.devices.bluetooth.genericattributeprofile as gatt
from winrt.windows.storage.streams import DataWriter

OTA_SERVICE_UUID = uuid.UUID("5833ff01-9b8b-5191-6142-22a4536ef123")
OTA_CMD_UUID     = uuid.UUID("5833ff02-9b8b-5191-6142-22a4536ef123")

async def main():
    print("[*] Поиск часов CHASY-STEALTH через BLE...")
    dev = await BleakScanner.find_device_by_name("CHASY-STEALTH", timeout=6.0)
    if not dev:
        print("[!] Часы CHASY-STEALTH не найдены в эфире.")
        sys.exit(1)

    print(f"[+] Найдены часы: {dev.name} [{dev.address}]")
    addr_int = int(dev.address.replace(":", ""), 16)
    ble_dev = await FutureLike(BluetoothLEDevice.from_bluetooth_address_async(addr_int))
    print(f"[+] WinRT устройство создано: {ble_dev.name}")

    print("[*] Прямой запрос OTA-сервиса (5833ff01)...")
    svc_res = await FutureLike(ble_dev.get_gatt_services_for_uuid_async(OTA_SERVICE_UUID))
    print(f"[*] Статус сервиса: {svc_res.status}")
    if svc_res.status != gatt.GattCommunicationStatus.SUCCESS:
        print(f"[!] Ошибка получения сервиса: {svc_res.status}")
        sys.exit(2)

    if not svc_res.services:
        print("[!] Сервис 5833ff01 не возвращен.")
        sys.exit(3)

    svc = svc_res.services[0]
    print(f"[+] Получен сервис: {svc.uuid}")

    print("[*] Прямой запрос характеристики OTA CMD (ff02)...")
    char_res = await FutureLike(svc.get_characteristics_for_uuid_async(OTA_CMD_UUID))
    print(f"[*] Статус характеристики: {char_res.status}")
    if char_res.status != gatt.GattCommunicationStatus.SUCCESS:
        print(f"[!] Ошибка получения характеристики: {char_res.status}")
        sys.exit(4)

    if not char_res.characteristics:
        print("[!] Характеристика 5833ff02 не найдена.")
        sys.exit(5)

    cmd_char = char_res.characteristics[0]
    print(f"[+] Получена характеристика CMD: {cmd_char.uuid}")

    # Формируем команду перехода в OTA режим: [0x01, 0x02, 0x01]
    writer = DataWriter()
    writer.write_bytes(bytes([0x01, 0x02, 0x01]))
    buf = writer.detach_buffer()

    print("[*] Отправка команды перевода в OTA режим [0x01, 0x02, 0x01]...")
    res = await FutureLike(cmd_char.write_value_async(buf))
    print(f"[+++] Результат отправки команды: {res}")

    print("[*] Ожидание перезагрузки часов в PPlusOTA (4 секунды)...")
    await asyncio.sleep(4.0)

    print("[*] Сканирование эфира на появление PPlusOTA...")
    devs = await BleakScanner.discover(timeout=5.0)
    for d in devs:
        name = d.name or "Unknown"
        if "OTA" in name or "63:62" in d.address.lower() or "CHASY" in name:
            print(f"[🎉] НАЙДЕНО: {name} [{d.address}]")

if __name__ == "__main__":
    asyncio.run(main())
