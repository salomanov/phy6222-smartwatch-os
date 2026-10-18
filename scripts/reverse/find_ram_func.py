with open('.backups/factory_dump.bin', 'rb') as f:
    data = f.read()

# Let's inspect the factory partition table at 0x010000 or 0x000000
import struct

# The OTA partitions in factory dump:
# Let's search for 'PHY6' tag in factory dump
pos = data.find(b'PHY6')
if pos != -1:
    tag, part_num, run_addr, total_size = struct.unpack('<4sIII', data[pos:pos+16])
    print(f"PHY6 at 0x{pos:06X}, parts={part_num}, total={total_size}")
    for i in range(part_num):
        flash_addr, size, part_run_addr, _ = struct.unpack('<IIII', data[pos+16+i*16:pos+32+i*16])
        print(f"Part {i}: Flash=0x{flash_addr:06X} Run=0x{part_run_addr:08X} Size={size}")
        if part_run_addr <= 0x1fff6218 < part_run_addr + size:
            offset_in_part = 0x1fff6218 - part_run_addr
            print(f"  --> 0x1fff6218 is in Part {i} at offset 0x{offset_in_part:04X} (Flash 0x{flash_addr+offset_in_part:06X})")
