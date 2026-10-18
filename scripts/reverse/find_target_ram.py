with open('.backups/factory_dump.bin', 'rb') as f:
    data = f.read()

import struct

target = 0x1FFF8F50

for i in range(0, len(data) - 4, 4):
    w = struct.unpack('<I', data[i:i+4])[0]
    if w == target or (target - 16 <= w <= target + 16):
        print(f"Ref at 0x{i:06X} (Flash 0x{0x11000000+i:08X}) -> 0x{w:08X}")
