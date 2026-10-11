import asyncio
from bleak import BleakClient

MAC = "62:62:15:07:07:15"

async def test_conn():
    print(f"[*] Подключение к часам {MAC}...")
    async with BleakClient(MAC, timeout=10.0) as client:
        print(f"[+] Подключено! client.is_connected = {client.is_connected}")
        print("[*] Перечисление служб и характеристик:")
        for service in client.services:
            print(f"\n[SERVICE] {service.uuid} ({service.description})")
            for char in service.characteristics:
                print(f"   [CHAR] {char.uuid} | Props: {','.join(char.properties)} | Handle: {char.handle}")
                
if __name__ == "__main__":
    asyncio.run(test_conn())
