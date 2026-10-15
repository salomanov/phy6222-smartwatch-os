import struct

hw1 = 0xF7FF
hw2 = 0xFFDA

s = (hw1 >> 10) & 1
imm10 = hw1 & 0x3FF
imm11 = hw2 & 0x7FF
j1 = (hw2 >> 13) & 1
j2 = (hw2 >> 11) & 1
i1 = not (j1 ^ s)
i2 = not (j2 ^ s)

offset = (s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1)
if offset & 0x1000000:
    offset -= 0x2000000
src = 0x11015234
dest = src + 4 + offset
print(f"dest: 0x{dest:08X}")
