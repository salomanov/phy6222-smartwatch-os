import asyncio
import struct
import time
import threading
import sys
import os
import serial
from bleak import BleakScanner, BleakClient

# Ensure line-buffered stdout for real-time log monitoring
if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(line_buffering=True)

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

async def find_and_connect(target_addr='62:62:15:07:07:15', timeout=60.0):
    found_event = asyncio.Event()
    target_device = [None]

    def detection_cb(dev, adv):
        addr_match = dev.address.upper() == target_addr.upper()
        name_match = bool(dev.name and any(k in dev.name.upper() for k in ['CHASY', 'OTA', 'PPLUS']))
        if addr_match or name_match:
            print(f"\n[+] Discovered watch in air: {dev.name or 'Unknown'} ({dev.address}), RSSI: {adv.rssi} dBm")
            target_device[0] = dev
            found_event.set()

    scanner = BleakScanner(detection_callback=detection_cb)
    print(f"[*] Starting continuous BLE scanner (waiting up to {int(timeout)}s for watch {target_addr})...")
    await scanner.start()
    try:
        start_time = time.time()
        while time.time() - start_time < timeout:
            try:
                await asyncio.wait_for(found_event.wait(), timeout=2.5)
                break
            except asyncio.TimeoutError:
                # Poke COM5 if available to wake watch or request status
                try:
                    s_wake = serial.Serial('COM5', 115200, timeout=0.05)
                    s_wake.write(b'\r\nstatus\r\n')
                    s_wake.close()
                except Exception:
                    pass
                elapsed = int(time.time() - start_time)
                print(f"  ...listening for BLE advertisement ({elapsed}s/{int(timeout)}s)...", flush=True)
    finally:
        await scanner.stop()

    if target_device[0] is not None:
        client = BleakClient(target_device[0], timeout=25.0, winrt={'use_cached_services': False})
        print(f"[+] Connecting to {target_device[0].address}...")
        await client.connect()
        return client
    return None

async def main():
    global stop_uart

    base_dir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    bin_path = os.path.join(base_dir, 'firmware', 'build', 'custom_firmware_ota.bin')
    print(f"[*] Loading OTA binary: {bin_path}...")
    partitions = parse_ota_bin(bin_path)
    total_bytes = sum(p['size'] for p in partitions)
    print(f"[+] Loaded {len(partitions)} partitions, total payload: {total_bytes} bytes.")
    for p in partitions:
        print(f"  Part {p['index']}: Flash=0x{p['flash_addr']:06X} Run=0x{p['run_addr']:08X} Size={p['size']} CRC=0x{p['checksum']:04X}")

    # Pre-emptively silence motor and LED before connection
    try:
        s_pre = serial.Serial('COM5', 115200, timeout=0.2)
        s_pre.write(b'pin 3 0\npin 0 1\n')
        time.sleep(0.05)
        s_pre.close()
    except Exception:
        pass

    client = None
    try:
        client = await find_and_connect(target_addr='62:62:15:07:07:15', timeout=45.0)
    except Exception as e:
        print(f"[!] Connect error: {e}")

    if not client or not client.is_connected:
        print("\n[!] Watch not detected in air or could not connect.")
        print("[!] Note: If watch battery is low, connect watch to USB charging or tap touch button to wake.")
        stop_uart = True
        sys.exit(1)

    print(f"[+] Connected successfully! MTU = {client.mtu_size}")

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
            sys.exit(1)
        print("[+] OTA Session started successfully! Display shows progress bar.")

        # Instantly silence the vibrator (P03=0) and LED (P00=1) via UART CLI to prevent power sag and RF noise
        try:
            ser_kill = serial.Serial('COM5', 115200, timeout=0.2)
            ser_kill.write(b'pin 3 0\npin 0 1\n')
            time.sleep(0.05)
            ser_kill.close()
            print("[+] Vibrator (P03=0) and LED (P00=1) silenced via UART CLI.", flush=True)
        except Exception as e:
            print(f"[!] UART kill-switch note: {e}", flush=True)

        # 2. Iterate partitions
        total_sent = 0
        chunk_size = 230  # Max ATT payload (MTU 240 - 3 byte ATT header = 237 max, 230 is safe)
        burst_accum = 0

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
                sys.exit(1)

            print(f"[+] Streaming data for Part {idx} (Write without response + burst flow-control)...")
            p_data = part['data']
            offset = 0

            while offset < len(p_data):
                chunk = p_data[offset:offset+chunk_size]
                offset += len(chunk)
                total_sent += len(chunk)
                burst_accum += len(chunk)

                # Send chunk with retry on transient WinRT BLE stack error
                written = False
                for w_try in range(8):
                    try:
                        await client.write_gatt_char(OTA_DATA_UUID, chunk, response=False)
                        written = True
                        break
                    except Exception as e:
                        print(f"  [WARN] Write error on offset {offset}: {e}, retrying {w_try+1}/8...")
                        await asyncio.sleep(0.05)

                if not written:
                    raise RuntimeError(f"Failed to write chunk at offset {offset}")

                pct = (total_sent * 100) // total_bytes
                if offset % 4600 < chunk_size or offset >= len(p_data):
                    print(f"  [PROGRESS] {pct}% ({total_sent}/{total_bytes} bytes sent, part {idx}: {offset}/{len(p_data)})")

                # Pacing between chunks
                await asyncio.sleep(0.012)

                # PhyPlus In-App OTA burst synchronization:
                # Every 3840 bytes (240 * 16), the watch writes to Flash and sends OTA_RSP_BLOCK_BURST (0x87)
                if burst_accum >= 3840 and total_sent < total_bytes:
                    burst_accum -= 3840
                    rsp = await wait_for_cmd(0x87, timeout=6.0)
                    if not rsp or rsp[0] != 0:
                        print(f"  [WARN] Burst ACK 0x87 not received in time (rsp: {rsp}), continuing safely...")

            # Allow BLE stack to process the final packet and calculate CRC
            await asyncio.sleep(0.15)

            # End of partition check: 0x83 for last partition, 0x85 for others
            exp_done = 0x83 if (idx + 1 == len(partitions)) else 0x85
            print(f"[*] Waiting for partition {idx} completion ACK (0x{exp_done:02X})...")
            rsp = await wait_for_cmd(exp_done, timeout=15.0)
            if not rsp or rsp[0] != 0:
                print(f"[!] Error on partition {idx} completion: {rsp}")
                stop_uart = True
                sys.exit(1)
            print(f"[+] Part {idx} verified & written successfully!")
            await asyncio.sleep(0.05)
                sys.exit(1)
            print(f"[+] Part {idx} verified & written successfully!")
            await asyncio.sleep(0.1)

        # 3. Finalize: send REBOOT command and IMMEDIATELY disconnect to terminate BLE connection events
        print("\n[+] All partitions transmitted! Sending REBOOT and disconnecting immediately...")
        reboot_cmd = bytes([0x04, 0x01])
        try:
            await client.write_gatt_char(OTA_CMD_UUID, reboot_cmd, response=False)
        except Exception as e:
            print(f"[!] Reboot send note: {e}")

        # Immediately terminate BLE connection so Link Layer stops firing connection-event interrupts
        try:
            await client.disconnect()
            print("[+] BLE disconnected cleanly! Watch is installing firmware and rebooting...")
        except Exception as e:
            print(f"[!] Disconnect note: {e}")

    finally:
        stop_uart = True

    print("\n[SUCCESS] OTA FLASH PROCEDURE COMPLETED 100%!")

if __name__ == '__main__':
    asyncio.run(main())
