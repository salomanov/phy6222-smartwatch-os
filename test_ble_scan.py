import asyncio
from bleak import BleakScanner

async def scan():
    print("[*] Сканирование BLE эфира с ПК (5 секунд)...")
    discovered = await BleakScanner.discover(timeout=5.0, return_adv=True)
    found = False
    for d, adv in discovered.values():
        name = d.name or adv.local_name or "Unknown"
        addr = d.address
        if "CHASY" in name or "OTA" in name or "62:62" in addr.lower() or "63:62" in addr.lower():
            print(f"[+++] НАЙДЕНЫ ЧАСЫ: name='{name}', MAC={addr}, RSSI={adv.rssi} dBm")
            found = True
            
    print(f"[*] Всего обнаружено {len(discovered)} BLE устройств вокруг ПК.")
    if not found:
        print("[!] Внимание: прямого совпадения с фильтром не было. Список видимых устройств:")
        for d, adv in list(discovered.values())[:10]:
            print(f"   - {d.name or adv.local_name} [{d.address}] (RSSI: {adv.rssi})")

if __name__ == "__main__":
    asyncio.run(scan())
