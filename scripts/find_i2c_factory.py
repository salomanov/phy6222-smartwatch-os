import struct

dump = open('dumps/flash_dump_20261014_224010.bin', 'rb').read()

# Search for SC7A20 or QMA6100 I2C addresses (0x18, 0x19, 0x12) or I2C functions
# In ARM Thumb, pins are loaded as immediate values or PC-relative loads.
# P16 = 16 (0x10), P17 = 17 (0x11), P19 = 19 (0x13).
print(f"Dump size: {len(dump)} bytes")

# Let's search for "sc7a20", "qma", "bma", "acc" in ASCII
for s in [b"SC7A20", b"sc7a20", b"QMA", b"qma", b"BMA", b"bma", b"LIS", b"lis"]:
    idx = 0
    while True:
        pos = dump.find(s, idx)
        if pos == -1: break
        print(f"Found {s} at offset 0x{pos:06X} (Flash: 0x{0x11000000+pos:08X})")
        idx = pos + 1

# Check references to 0x11037FA0 and 0x11037FA8 from PINMAP.md
for target_addr in [0x11037FA0, 0x11037FA8, 0x1103137C, 0x11031384]:
    offset = target_addr - 0x11000000
    if 0 <= offset < len(dump):
        chunk = dump[offset:offset+16]
        print(f"Addr 0x{target_addr:08X} (offset 0x{offset:06X}): {chunk.hex()}")
