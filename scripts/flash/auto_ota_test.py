import asyncio
import sys
import uuid
import time
from bleak import BleakScanner
from bleak.backends.winrt.client import FutureLike
from winrt.windows.devices.bluetooth import BluetoothLEDevice
import winrt.windows.devices.bluetooth.genericattributeprofile as gatt
from winrt.windows.storage.streams import DataWriter

OTA_SERVICE_UUID = uuid.UUID("5833ff01-9b8b-5191-6142-22a4536ef123")
OTA_CMD_UUID     = uuid.UUID("5833ff02-9b8b-5191-6142-22a4536ef123")

async def main():
    print("[*] 1. Поиск устройства CHASY-STEALTH...")
    dev = await BleakScanner.find_device_by_name("CHASY-STEALTH", timeout=5.0)
    if not dev:
        print("[!] CHASY-STEALTH не найден в эфире.")
        sys.exit(1)

    print(f"[+] 2. Найдено устройство: {dev.name} [{dev.address}]")
    addr_int = int(dev.address.replace(":", ""), 16)
    ble_dev = await FutureLike(BluetoothLEDevice.from_bluetooth_address_async(addr_int))

    print("[*] 3. Запрос GATT OTA службы (5833ff01)...")
    svc_res = await FutureLike(ble_dev.get_gatt_services_for_uuid_async(OTA_SERVICE_UUID))
    if svc_res.status != gatt.GattCommunicationStatus.SUCCESS or not svc_res.services:
        print(f"[!] Не удалось получить сервис: {svc_res.status}")
        sys.exit(2)

    svc = svc_res.services[0]
    char_res = await FutureLike(svc.get_characteristics_for_uuid_async(OTA_CMD_UUID))
    if char_res.status != gatt.GattCommunicationStatus.SUCCESS or not char_res.characteristics:
        print(f"[!] Не удалось получить характеристику: {char_res.status}")
        sys.exit(3)

    cmd_char = char_res.characteristics[0]
    print("[+] 4. Характеристика OTA CMD получена.")

    # Команда перехода в режим OTA с немедленным сбросом: [0x01, 0x02, 0x00]
    # (mode 2 = OTA_MODE_OTA, flag 0 = graceful immediate reset by device)
    writer = DataWriter()
    writer.write_bytes(bytes([0x01, 0x02, 0x00]))
    buf = writer.detach_buffer()

    print("[*] 5. Отправка команды перехода в OTA режим [0x01, 0x02, 0x00]...")
    try:
        wr_res = await FutureLike(cmd_char.write_value_with_result_async(buf))
        print(f"[+] Результат записи: status = {wr_res.status}")
    except Exception as e:
        print(f"[i] Соединение разорвано чипом при перезагрузке (ожидаемо): {e}")

    # Закрываем GATT со стороны хоста
    try:
        svc.close()
        ble_dev.close()
    except:
        pass

    print("[*] 6. Ожидание 3 секунды...")
    await asyncio.sleep(3.0)

    print("[*] 7. Сканирование BLE эфира на появление загрузчика PPlusOTA...")
    for attempt in range(3):
        devs = await BleakScanner.discover(timeout=4.0)
        found = False
        for d in devs:
            name = d.name or "Unknown"
            print(f"   - {name} [{d.address}]")
            if "OTA" in name or "PPlus" in name or "63:62" in d.address.lower():
                print(f"[🎉🎉🎉] ЗАГРУЗЧИК PPlusOTA ОБНАРУЖЕН: {name} [{d.address}]!")
                found = True
                return
        if not found:
            print(f"[...] Попытка {attempt+1}/3: загрузчик еще не появился, ждем...")
            await asyncio.sleep(1.5)

if __name__ == "__main__":
    asyncio.run(main())
