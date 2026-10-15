import struct

with open('.backups/factory_dump.bin', 'rb') as f:
    data = f.read()

target = 0x11015208

# ARMv6-M (Thumb) BL instruction:
# hw1: 1111 0 [10-bit imm] -> 0xF000 | (imm10)
# hw2: 1111 1 [11-bit imm] -> 0xF800 | (imm11)
# sign extend (imm10:imm11:0) from 22 bits to 32 bits

for i in range(0, len(data) - 4, 2):
    hw1, hw2 = struct.unpack('<HH', data[i:i+4])
    if (hw1 & 0xF800) == 0xF000 and (hw2 & 0xF800) == 0xF800:
        imm10 = hw1 & 0x7FF
        imm11 = hw2 & 0x7FF
        # in ARMv6-M:
        offset = (imm10 << 12) | (imm11 << 1)
        if offset & 0x400000:
            offset -= 0x800000
        dest = (0x11000000 + i + 4) + offset
        if dest == target:
            print(f"Call to hal_gpio_fmux_set from 0x{0x11000000+i:08X} (offset 0x{i:06X})")
