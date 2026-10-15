with open('.backups/factory_dump.bin', 'rb') as f:
    data = f.read()

# Let's inspect bytes before and after 0x034B00
start = 0x034A00
end = 0x034C00
print(f"Bytes around 0x034B00:")
for offset in range(start, end, 16):
    chunk = data[offset:offset+16]
    hex_str = ' '.join(f"{b:02X}" for b in chunk)
    ascii_str = ''.join(chr(b) if 32 <= b < 127 else '.' for b in chunk)
    print(f"{offset:06X}: {hex_str}  |  {ascii_str}")
