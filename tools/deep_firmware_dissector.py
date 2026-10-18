import struct
import os
import re

DUMP_PATH = r"E:\Projects\Watch\.backups\factory_dump.bin"
FLASH_BASE = 0x11000000

with open(DUMP_PATH, "rb") as f:
    raw_data = f.read()

print(f"Loaded {len(raw_data)} bytes of factory firmware.")

def read_u32(addr):
    off = addr - FLASH_BASE
    if 0 <= off <= len(raw_data) - 4:
        return struct.unpack("<I", raw_data[off:off+4])[0]
    return None

def read_u16(addr):
    off = addr - FLASH_BASE
    if 0 <= off <= len(raw_data) - 2:
        return struct.unpack("<H", raw_data[off:off+2])[0]
    return None

# ==============================================================================
# 1. TRACE TRAMPOLINE TABLE AT 0x11020000
# ==============================================================================
trampolines = {}
for off in range(0x020000, 0x020500, 12):
    # push {r0, r1}; ldr r0, [pc, #4]; str r0, [sp, #4]; pop {r0, pc}; target
    p1 = struct.unpack("<H", raw_data[off:off+2])[0]
    if p1 == 0xb403:
        target = struct.unpack("<I", raw_data[off+8:off+12])[0]
        func_addr = FLASH_BASE + off
        trampolines[func_addr] = target

print(f"Discovered {len(trampolines)} trampoline functions in table at 0x11020000.")

# ==============================================================================
# 2. SCAN FOR ALL GPIO PIN ACCESSES IN MAIN APPLICATION (0x11020000 - 0x11040000)
# ==============================================================================
# Known trampoline wrappers:
# 0x11020060 -> hal_gpio_cfg_analog_io / hal_gpio_pull_set?
# 0x1102006C -> hal_gpio_pin_init (direction)
# 0x11020078 -> hal_gpio_pull_set
# 0x11020090 -> hal_gpio_write
# 0x1102009C -> hal_gpio_read / wakeup
# Let's inspect all BL instructions targeting 0x11020000-0x11020500

gpio_accesses = []

# Disassemble BL instructions:
# BL is a 32-bit Thumb instruction:
# 1st half: 11110xxxxxxxxxxx (0xF000 - 0xF7FF)
# 2nd half: 11111xxxxxxxxxxx (0xF800 - 0xFFFF)
app_start = 0x020000
app_end = 0x03E000

for off in range(app_start, app_end - 4, 2):
    h1 = struct.unpack("<H", raw_data[off:off+2])[0]
    h2 = struct.unpack("<H", raw_data[off+2:off+4])[0]
    
    if (h1 & 0xF800) == 0xF000 and (h2 & 0xD000) == 0xD000:
        # Calculate BL target
        s = (h1 >> 10) & 1
        imm10 = h1 & 0x3FF
        j1 = (h2 >> 13) & 1
        j2 = (h2 >> 11) & 1
        imm11 = h2 & 0x7FF
        
        i1 = ~(j1 ^ s) & 1
        i2 = ~(j2 ^ s) & 1
        offset = (s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1)
        if s:
            offset -= (1 << 25)
            
        pc = FLASH_BASE + off + 4
        target = pc + offset
        
        if target in trampolines:
            # Look at previous 3 instructions to identify arguments (r0, r1)
            # movs r0, #imm (0x20xx), movs r1, #imm (0x21xx)
            args = {}
            for back_off in range(off - 8, off, 2):
                if back_off >= 0:
                    instr = struct.unpack("<H", raw_data[back_off:back_off+2])[0]
                    if (instr & 0xFF00) == 0x2000:
                        args['r0'] = instr & 0xFF
                    elif (instr & 0xFF00) == 0x2100:
                        args['r1'] = instr & 0xFF
                    elif (instr & 0xFF00) == 0x2200:
                        args['r2'] = instr & 0xFF
            
            t_target = trampolines[target]
            gpio_accesses.append((FLASH_BASE + off, target, t_target, args))

print(f"\nDiscovered {len(gpio_accesses)} BL calls to trampolines.")

# Group by target trampoline
calls_by_target = {}
for caller, bl_target, t_target, args in gpio_accesses:
    calls_by_target.setdefault(bl_target, []).append((caller, t_target, args))

for bl_target, call_list in sorted(calls_by_target.items()):
    t_target = trampolines[bl_target]
    print(f"\n--- Trampoline [0x{bl_target:08X}] -> 0x{t_target:08X} ({len(call_list)} calls) ---")
    pin_counts = {}
    sample_calls = []
    for caller, _, args in call_list:
        if 'r0' in args:
            pin = args['r0']
            pin_counts[pin] = pin_counts.get(pin, 0) + 1
        if len(sample_calls) < 8:
            sample_calls.append((caller, args))
    
    if pin_counts:
        print(f"  Pins referenced in r0: {sorted(pin_counts.items())}")
    for caller, args in sample_calls:
        print(f"    0x{caller:08X}: args={args}")

# ==============================================================================
# 3. SEARCH FOR ALL EXPLICIT PIN MASKS (1 << pin)
# ==============================================================================
print("\n=== 3. GPIO PIN BITMASK ANALYSIS (1 << Pin) ===")
pin_masks = {1 << p: p for p in range(35)}
for off in range(0x005000, 0x03E000, 4):
    val = struct.unpack("<I", raw_data[off:off+4])[0]
    if val in pin_masks and bin(val).count("1") == 1:
        pin = pin_masks[val]
        # Only report if it is in an interesting range
        addr = FLASH_BASE + off
        # check surrounding instructions or data
        # print(f"  [0x{addr:08X}] Bitmask for Pin P{pin:02d} (0x{val:08X})")

# ==============================================================================
# 4. OSAL TASK DISCOVERY & BLE SERVICES
# ==============================================================================
print("\n=== 4. BLE PROFILE & GATT SERVICE SEARCH ===")
# Search for standard BLE UUIDs: 0xFEE7, 0xFEE8, 0xFE80, 0x180D, 0x180F, 0x180A, 0x1800
uuids = {
    0xFEE7: "WeChat AirSync / Step Sync (0xFEE7)",
    0xFEE8: "PhyPlus OTA Service (0xFEE8)",
    0xFE80: "Custom Proprietary Service (0xFE80)",
    0xFE81: "Custom TX Char (0xFE81)",
    0xFE82: "Custom RX Char (0xFE82)",
    0x180D: "Heart Rate Service (0x180D)",
    0x180F: "Battery Service (0x180F)",
    0x180A: "Device Information Service (0x180A)",
    0x1800: "Generic Access (0x1800)",
}

for off in range(0, len(raw_data) - 2, 2):
    val = struct.unpack("<H", raw_data[off:off+2])[0]
    if val in uuids:
        addr = FLASH_BASE + off
        print(f"  [0x{addr:08X}] UUID 0x{val:04X} -> {uuids[val]}")

