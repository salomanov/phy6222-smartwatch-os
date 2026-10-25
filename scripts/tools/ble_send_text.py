import asyncio
import sys
import argparse
from bleak import BleakScanner, BleakClient

SIMPLE_CHAR1_UUID = '0000fff1-0000-1000-8000-00805f9b34fb'

COLOR_MAP = {
    'W': 'W', 'WHITE': 'W',
    'G': 'G', 'GREEN': 'G',
    'Y': 'Y', 'YELLOW': 'Y',
    'C': 'C', 'CYAN': 'C',
    'R': 'R', 'RED': 'R',
    'M': 'M', 'MAGENTA': 'M',
    'B': 'B', 'BLUE': 'B',
    'O': 'O', 'ORANGE': 'O',
}

def split_text_into_safe_chunks(text: str, first_max_bytes: int = 9, rest_max_bytes: int = 13):
    chunks = []
    current = ""
    max_b = first_max_bytes
    for ch in text:
        cand = current + ch
        if len(cand.encode('utf-8')) > max_b:
            if current:
                chunks.append(current)
            current = ch
            max_b = rest_max_bytes
        else:
            current = cand
    if current:
        chunks.append(current)
    return chunks

async def send_message(text: str, color: str = 'White', scale: int = 1, target_addr: str = '62:62:15:07:07:15'):
    color_char = COLOR_MAP.get(color.upper(), 'W')
    scale_char = '2' if scale == 2 else '1'
    utf8_bytes = text.encode('utf-8')

    print("====================================================")
    print("   LP715 BLE NOTIFICATION / PAGER (Python)")
    print("====================================================")
    print(f"[*] Target Watch   : {target_addr}")
    print(f"[*] Message Text   : '{text}' ({len(utf8_bytes)} UTF-8 bytes)")
    print(f"[*] Color / Scale  : {color} ({color_char}) / Scale {scale_char}")

    found_event = asyncio.Event()
    target_dev = [None]

    def detection_cb(dev, adv):
        if dev.address.upper() == target_addr.upper() or (dev.name and 'CHASY' in dev.name.upper()):
            target_dev[0] = dev
            found_event.set()

    scanner = BleakScanner(detection_callback=detection_cb)
    await scanner.start()
    try:
        await asyncio.wait_for(found_event.wait(), timeout=10.0)
    except asyncio.TimeoutError:
        print("[!] Discovery timeout! Is smartwatch advertising?")
        return False
    finally:
        await scanner.stop()

    if not target_dev[0]:
        print("[!] Device not found in air.")
        return False

    async with BleakClient(target_dev[0], timeout=15.0) as client:
        print(f"[+] Connected to {target_dev[0].name or target_addr}!")

        def make_pkt(payload: bytes) -> bytes:
            return payload.ljust(16, b'\x00')[:16]

        single_hdr = f"MSG:{color_char}:{scale_char}:".encode('ascii')
        if len(single_hdr) + len(utf8_bytes) <= 16:
            pkt = make_pkt(single_hdr + utf8_bytes)
            print(f"[*] Sending single packet: {pkt}...")
            await client.write_gatt_char(SIMPLE_CHAR1_UUID, pkt, response=True)
        else:
            chunks = split_text_into_safe_chunks(text, first_max_bytes=9, rest_max_bytes=13)
            print(f"[*] Split into {len(chunks)} safe UTF-8 chunks:")

            for i, chunk_text in enumerate(chunks):
                c_bytes = chunk_text.encode('utf-8')
                if i == 0:
                    hdr = f"M+:{color_char}:{scale_char}:".encode('ascii')
                    pkt = make_pkt(hdr + c_bytes)
                    print(f"  -> Chunk 1 (Start): '{chunk_text}' ({len(c_bytes)}b)")
                    await client.write_gatt_char(SIMPLE_CHAR1_UUID, pkt, response=True)
                elif i == len(chunks) - 1:
                    hdr = b"M.:"
                    pkt = make_pkt(hdr + c_bytes)
                    print(f"  -> Chunk {i+1} (End & Display): '{chunk_text}' ({len(c_bytes)}b)")
                    await client.write_gatt_char(SIMPLE_CHAR1_UUID, pkt, response=True)
                else:
                    hdr = b"M~:"
                    pkt = make_pkt(hdr + c_bytes)
                    print(f"  -> Chunk {i+1} (Cont): '{chunk_text}' ({len(c_bytes)}b)")
                    await client.write_gatt_char(SIMPLE_CHAR1_UUID, pkt, response=True)
                await asyncio.sleep(0.06)

        print("====================================================")
        print("[+] SUCCESS! Message displayed on smartwatch.")
        print("    Tap touch button to dismiss.")
        print("====================================================")
        return True

def main():
    parser = argparse.ArgumentParser(description="Send text messages to LP715 smartwatch over BLE")
    parser.add_argument("text", nargs="?", default="Привет, мир!", help="Text message to display")
    parser.add_argument("-c", "--color", default="White", help="Color: White, Green, Yellow, Cyan, Red, Magenta, Blue, Orange")
    parser.add_argument("-s", "--scale", type=int, choices=[1, 2], default=1, help="Font scale: 1 (small) or 2 (large)")
    parser.add_argument("-a", "--addr", default="62:62:15:07:07:15", help="Target MAC address")
    args = parser.parse_args()

    asyncio.run(send_message(args.text, args.color, args.scale, args.addr))

if __name__ == '__main__':
    main()
