import asyncio
import uuid
from bleak import BleakScanner
from bleak.backends.winrt.client import FutureLike
from winrt.windows.devices.bluetooth import BluetoothLEDevice
from winrt.windows.storage.streams import DataWriter

OTA_SERVICE_UUID = uuid.UUID("5833ff01-9b8b-5191-6142-22a4536ef123")
OTA_CMD_UUID     = uuid.UUID("5833ff02-9b8b-5191-6142-22a4536ef123")

async def main():
    print("[*] 1. Поиск CHASY-STEALTH...")
    dev = await BleakScanner.find_device_by_name("CHASY-STEALTH", timeout=5.0)
    if not dev:
        print("[!] CHASY-STEALTH не найден.")
        return

    print(f"[+] Найден: {dev.name} [{dev.address}]")
    addr_int = int(dev.address.replace(":", ""), 16)
    ble_dev = await FutureLike(BluetoothLEDevice.from_bluetooth_address_async(addr_int))
    svc_res = await FutureLike(ble_dev.get_gatt_services_for_uuid_async(OTA_SERVICE_UUID))
    svc = svc_res.services[0]
    char_res = await FutureLike(svc.get_characteristics_for_uuid_async(OTA_CMD_UUID))
    cmd_char = char_res.characteristics[0]

    # Команда перехода в OTA режим: [0x01, 0x02, 0x01]
    writer = DataWriter()
    writer.write_bytes(bytes([0x01, 0x02, 0x01]))
    buf = writer.detach_buffer()

    print("[*] 2. Отправка команды START_OTA [0x01, 0x02, 0x01]...")
    wr_res = await FutureLike(cmd_char.write_value_with_result_async(buf))
    print(f"[+] Результат записи: status = {wr_res.status}")

    # Явное закрытие GATT-сессии для инициации сброса
    print("[*] 3. Разрыв GATT сессии (чтобы чип инициировал ресет в OTA)...")
    svc.close()
    ble_dev.close()
    del svc
    del ble_dev

    print("[*] 4. Ожидание 3 секунды...")
    await asyncio.sleep(3.0)

    print("[*] 5. Сканирование BLE эфира на появление PPlusOTA...")
    devs = await BleakScanner.discover(timeout=5.0)
    for d in devs:
        print(f"   - {d.name} [{d.address}]")

if __name__ == "__main__":
    asyncio.run(main())
