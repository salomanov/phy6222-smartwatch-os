import asyncio
import struct
import time
import threading
import serial
from bleak import BleakScanner, BleakClient

OTA_SERVICE_UUID = '5833ff01-9b8b-5191-6142-22a4536ef123'
OTA_CMD_UUID     = '5833ff02-9b8b-5191-6142-22a4536ef123'
OTA_RSP_UUID     = '5833ff03-9b8b-5191-6142-22a4536ef123'
OTA_DATA_UUID    = '5833ff04-9b8b-5191-6142-22a4536ef123'

stop_uart = False

def uart_monitor():
    global stop_uart
    try:
        ser = serial.Serial('COM5', 115200, timeout=0.1)
        print("[UART] Connected to COM5 monitor.")
        while not stop_uart:
            try:
                line = ser.readline()
                if line:
                    decoded = line.decode('utf-8', errors='replace').strip()
                    if decoded:
                        print(f"  [CHIP UART] {decoded}")
            except Exception:
                pass
        ser.close()
    except Exception as e:
        print(f"[UART] Warning: Could not open COM5 ({e})")

# Phyplus CRC16 calculation
CRC16_TABLE = [
    0x0000, 0xCC01, 0xD801, 0x1400, 0xF001, 0x3C00, 0x2800, 0xE401,
    0xA001, 0x6C00, 0x7800, 0xB401, 0x5000, 0x9C01, 0x8801, 0x4400
]

def calc_phy_crc16(data: bytes, seed: int = 0) -> int:
    crc = seed
    for b in data:
        temp = CRC16_TABLE[crc & 0xF]
        crc = (crc >> 4) & 0x0FFF
        crc = crc ^ temp ^ CRC16_TABLE[b & 0xF]
        temp = CRC16_TABLE[crc & 0xF]
        crc = (crc >> 4) & 0x0FFF
        crc = crc ^ temp ^ CRC16_TABLE[(b >> 4) & 0xF]
    return crc

def parse_ota_bin(filepath):
    with open(filepath, 'rb') as f:
        data = f.read()
    tag, part_num, run_addr, total_size = struct.unpack('<4sIII', data[:16])
    if tag != b'PHY6':
        raise ValueError(f"Invalid OTA header tag: {tag}")
    
    partitions = []
    data_offset = 256
    for i in range(part_num):
        flash_addr, size, part_run_addr, _ = struct.unpack('<IIII', data[16+i*16:32+i*16])
        part_data = data[data_offset:data_offset+size]
        data_offset += size
        if size % 4 != 0:
            data_offset += (4 - (size % 4))
        
        # Calculate real CRC16 as expected by firmware
        chk = calc_phy_crc16(part_data)
        partitions.append({
            'index': i,
            'flash_addr': flash_addr,
            'run_addr': part_run_addr,
            'size': size,
            'checksum': chk,
            'data': part_data
        })
    return partitions

async def find_watch():
    print("[*] Finding CHASY-STEALTH...")
    for attempt in range(1, 10):
        device = await BleakScanner.find_device_by_filter(
            lambda d, adv: d.address.upper() == '62:62:15:07:07:15' or (d.name and any(k in d.name.upper() for k in ['CHASY', 'OTA', 'PPLUS'])),
            timeout=4.0
        )
        if device:
            return device
        print(f"  Attempt {attempt} failed, retrying scan...")
        await asyncio.sleep(0.5)
    return None

async def main():
    global stop_uart
    # If COM5 is connected, monitor it, otherwise ignore
    u_thread = threading.Thread(target=uart_monitor, daemon=True)
    u_thread.start()
    time.sleep(0.5)

    bin_path = 'firmware/build/custom_firmware_ota.bin'
    print(f"[*] Loading OTA binary: {bin_path}...")
    partitions = parse_ota_bin(bin_path)
    total_bytes = sum(p['size'] for p in partitions)
    print(f"[+] Loaded {len(partitions)} partitions, total payload: {total_bytes} bytes.")
    for p in partitions:
        print(f"  Part {p['index']}: Flash=0x{p['flash_addr']:06X} Run=0x{p['run_addr']:08X} Size={p['size']} CRC=0x{p['checksum']:04X}")

    client = None
    for conn_try in range(1, 10):
        print(f"\n[*] Scan & Connect attempt {conn_try}/9...")
        device = await BleakScanner.find_device_by_filter(
            lambda d, adv: d.address.upper() == '62:62:15:07:07:15' or (d.name and 'CHASY' in d.name),
            timeout=5.0
        )
        if not device:
            print(f"  Attempt {conn_try}: watch not seen in air yet, retrying in 1s...")
            await asyncio.sleep(1.0)
            continue

        print(f"[+] Found {device.name} ({device.address}), attempting connection...")
        try:
            client = BleakClient(device, timeout=20.0)
            await client.connect()
            if client.is_connected:
                print(f"[+] Connected successfully! MTU = {client.mtu_size}")
                break
        except Exception as e:
            print(f"  Connection attempt {conn_try} failed: {e}")
            await asyncio.sleep(1.5)

    if not client or not client.is_connected:
        print("[!] Failed to establish BLE connection after all retries.")
        stop_uart = True
        return

    try:
        rsp_queue = []

        def on_notification(sender, data):
            raw = list(data)
            status = raw[0] if len(raw) > 0 else -1
            cmd = raw[1] if len(raw) > 1 else -1
            if cmd != 0x87:
                print(f"  << [BLE NOTIF] status=0x{status:02X}, cmd=0x{cmd:02X}")
                rsp_queue.append(raw)

        await client.start_notify(OTA_RSP_UUID, on_notification)
        await asyncio.sleep(0.5)

        async def wait_for_cmd(exp_cmd, timeout=8.0):
            t_end = time.time() + timeout
            while time.time() < t_end:
                for idx, r in enumerate(rsp_queue):
                    if len(r) > 1 and r[1] == exp_cmd:
                        rsp_queue.pop(idx)
                        return r
                await asyncio.sleep(0.02)
            return None

        # 1. Start OTA: [cmd=0x01, sector_num, burst_size=16, total_fw_size(4)]
        print(f"[*] Sending OTA_CMD_START_OTA (partitions: {len(partitions)}, total: {total_bytes} bytes)...")
        start_cmd = struct.pack('<BBBI', 0x01, len(partitions), 16, total_bytes)
        await client.write_gatt_char(OTA_CMD_UUID, start_cmd, response=True)
        rsp = await wait_for_cmd(0x81, timeout=8.0)
        if not rsp or rsp[0] != 0:
            print(f"[!] Start OTA rejected: {rsp}")
            stop_uart = True
            return
        print("[+] OTA Session started successfully! Display shows progress bar.")

        # 2. Iterate partitions
        total_sent = 0
        chunk_size = 230  # Max ATT payload (MTU 240 - 3 byte ATT header = 237 max, 230 is safe)

        for part in partitions:
            idx = part['index']
            print(f"\n[*] Sending PARTITION_INFO for Part {idx} (size: {part['size']} bytes, CRC: 0x{part['checksum']:04X})...")
            part_cmd = struct.pack('<BBIIII',
                                   0x02,
                                   idx,
                                   part['flash_addr'],
                                   part['run_addr'],
                                   part['size'],
                                   part['checksum'])
            await client.write_gatt_char(OTA_CMD_UUID, part_cmd, response=True)
            rsp = await wait_for_cmd(0x84, timeout=8.0)
            if not rsp or rsp[0] != 0:
                print(f"[!] Partition {idx} info rejected: {rsp}")
                stop_uart = True
                return

            print(f"[+] Streaming data for Part {idx} (Write with response for 100% reliability)...")
            p_data = part['data']
            offset = 0

            while offset < len(p_data):
                chunk = p_data[offset:offset+chunk_size]
                offset += len(chunk)
                total_sent += len(chunk)

                # Send chunk with retry on transient WinRT BLE stack error
                written = False
                for w_try in range(8):
                    try:
                        await client.write_gatt_char(OTA_DATA_UUID, chunk, response=True)
                        written = True
                        break
                    except Exception as e:
                        print(f"  [WARN] Write error on offset {offset}: {e}, retrying {w_try+1}/8...")
                        await asyncio.sleep(0.3)

                if not written:
                    raise RuntimeError(f"Failed to write chunk at offset {offset}")

                pct = (total_sent * 100) // total_bytes
                if offset % 4600 < chunk_size or offset >= len(p_data):
                    print(f"  [PROGRESS] {pct}% ({total_sent}/{total_bytes} bytes sent, part {idx}: {offset}/{len(p_data)})")

                # Pacing aligned with BLE connection interval (30-40ms) + Flash write
                await asyncio.sleep(0.035)

            # Allow BLE stack to process the final packet and calculate CRC
            await asyncio.sleep(0.2)

            # End of partition check: 0x83 for last partition, 0x85 for others
            exp_done = 0x83 if (idx + 1 == len(partitions)) else 0x85
            print(f"[*] Waiting for partition {idx} completion ACK (0x{exp_done:02X})...")
            rsp = await wait_for_cmd(exp_done, timeout=15.0)
            if not rsp or rsp[0] != 0:
                print(f"[!] Error on partition {idx} completion: {rsp}")
                stop_uart = True
                return
            print(f"[+] Part {idx} verified & written successfully!")
            await asyncio.sleep(0.1)

        # 3. Finalize & Reboot
        print("\n[+] All partitions transmitted! Sending REBOOT command...")
        reboot_cmd = bytes([0x04, 0x01])
        try:
            await client.write_gatt_char(OTA_CMD_UUID, reboot_cmd, response=False)
            print("[+] Reboot command sent! Watch restarting into new firmware...")
        except Exception as e:
            print(f"[!] Reboot notification note: {e}")
        await asyncio.sleep(2)

    finally:
        try:
            await client.disconnect()
        except Exception:
            pass
        stop_uart = True

    print("\n[SUCCESS] OTA FLASH PROCEDURE COMPLETED 100%!")

if __name__ == '__main__':
    asyncio.run(main())
