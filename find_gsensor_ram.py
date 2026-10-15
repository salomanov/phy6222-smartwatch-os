import struct

with open('.backups/factory_dump.bin', 'rb') as f:
    data = f.read()

# Let's inspect 0x11020f10
# In disasm:
# 11020f06: 4802 ldr r0, [pc, #8] @ (0x11020f10)
# PC is 11020f06 + 4 = 11020f0a. 11020f0a aligned to 4 = 11020f08 + 8 = 11020f10.
# At 0x11020f10 in factory dump:
offset = 0x020f10
val = struct.unpack('<I', data[offset:offset+4])[0]
print(f"Address at 0x11020f10: 0x{val:08X}")
ram_target = val - 6
print(f"RAM target: 0x{ram_target:08X}")

# Let's search for references to val or ram_target
for i in range(0, len(data) - 4, 4):
    w = struct.unpack('<I', data[i:i+4])[0]
    if w == val or w == ram_target or abs(w - val) < 32:
        print(f"Match at 0x{i:06X} (Flash 0x{0x11000000+i:08X}) -> 0x{w:08X}")
