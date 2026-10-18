import os
import struct
import re

DUMP_PATH = r"E:\Projects\Watch\.backups\factory_dump.bin"
FLASH_BASE = 0x11000000

with open(DUMP_PATH, "rb") as f:
    data = f.read()

print(f"[*] Loaded factory_dump.bin: {len(data)} bytes")

# 1. Extract all ASCII strings >= 4 chars
print("\n=== 1. ASCII STRINGS EXTRACTION ===")
strings = []
for match in re.finditer(b"[\x20-\x7E]{4,}", data):
    offset = match.start()
    addr = FLASH_BASE + offset
    s = match.group().decode("latin1", errors="replace")
    strings.append((addr, offset, s))

print(f"[*] Total strings found: {len(strings)}")
keywords = ["i2c", "acc", "sc7", "qma", "lis", "sensor", "step", "hr", "led", "vib", "motor", "key", "btn", "touch", "disp", "lcd", "st7735", "bkl", "gpio", "pin", "ota", "sleep", "aon"]
for addr, offset, s in strings:
    s_lower = s.lower()
    if any(k in s_lower for k in keywords):
        print(f"  [0x{addr:08X}] (+0x{offset:06X}): {s}")

# 2. Search for Peripheral Addresses
# AP_GPIOA: 0x40008000
# AP_IOMUX: 0x40003800
# AP_AON:   0x4000F000
# AP_SPI0:  0x40006000
# AP_SPI1:  0x40007000
# AP_UART0: 0x40004000
# AP_I2C0:  0x40005000
# AP_I2C1:  0x40012000
print("\n=== 2. PERIPHERAL BASE REFERENCES ===")
periphs = {
    0x40008000: "AP_GPIOA",
    0x40003800: "AP_IOMUX",
    0x4000F000: "AP_AON",
    0x40006000: "AP_SPI0",
    0x40007000: "AP_SPI1",
    0x40004000: "AP_UART0",
    0x40005000: "AP_I2C0",
    0x40012000: "AP_I2C1",
}

for offset in range(0, len(data) - 4, 4):
    val = struct.unpack("<I", data[offset:offset+4])[0]
    for p_addr, p_name in periphs.items():
        if val == p_addr or (p_addr <= val <= p_addr + 0x200):
            flash_addr = FLASH_BASE + offset
            print(f"  [0x{flash_addr:08X}] -> {p_name} (+0x{val - p_addr:X}) [val=0x{val:08X}]")

