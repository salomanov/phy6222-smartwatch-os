with open('.backups/factory_dump.bin', 'rb') as f:
    data = f.read()

import struct

# AP_I2C0 is at 0x40005000, AP_I2C1 is at 0x40005400
# Also FMUX base is 0x40003800
for i in range(0, len(data) - 4, 4):
    w = struct.unpack('<I', data[i:i+4])[0]
    if w in (0x40005000, 0x40005400, 0x40003800):
        print(f"I2C reg ref at 0x{i:06X} (Flash 0x{0x11000000+i:08X}) -> 0x{w:08X}")
