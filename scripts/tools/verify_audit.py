"""Audit: does the built image match what the in-app BLE OTA installer expects?

Replaces the old check that compared a stale, unused `firmware/src/wire_boot_header.h`
against the fresh hex. That file was dead code (nothing included it) and went red after
every growth of the image, while the real risk is different: `ota_install_firmware_and_reboot()`
in `firmware/src/ota_inapp.c` copies partitions to hardcoded flash addresses and writes a
fixed entry point, and the staging area is a fixed 160 KB.

This script verifies, against the freshly built hex:
  1. the wire boot header has the expected 320-byte size;
  2. every flash address used by the installer exists in the image;
  3. the installer entry point matches a real segment run address;
  4. the OTA payload fits into the staging area with a safety margin.

Run:  python scripts/tools/verify_audit.py
Exit: 0 = PASS, 1 = FAIL
"""

import sys

sys.path.append('tools')
from rdwr_phy62x2 import phyflasher, ParseHexFile

HEX_PATH = 'firmware/build/custom_firmware.hex'

REGION_BASE = 0x11000000   # XIP flash base; segment "flash offset" is added to it
STAGING_BYTES = 0x28000    # ota_inapp.c: OTA_TEMP_BASE + 0x28000 (160 KB)
STAGING_MARGIN = 0x1000    # keep at least one spare 4 KB sector
ENTRY_ADDR = 0x1FFF1838    # ota_inapp.c: p32[2] value written into the boot header

# ota_inapp.c: ota_install_firmware_and_reboot() -- absolute flash targets, part 0..3
INSTALLER_TARGETS = [0x11010100, 0x11020000, 0x11005000, 0x11005400]

errors = []

phy = object.__new__(phyflasher)
phy.tg7100 = False
hp = ParseHexFile(HEX_PATH)
hdr = phyflasher.HexfHeader(phy, hp)

segments = []
for idx, ihp in enumerate(hp):
    if idx == 0:  # first entry is a pseudo-segment (header), same as before
        continue
    segments.append({
        'run': ihp[0],
        'flash': REGION_BASE + ihp[2],
        'size': len(ihp[1]),
    })

print(f"Wire header: {len(hdr)} bytes (expected 320), segments: {len(segments)}")
for i, s in enumerate(segments):
    print(f"  Part {i}: Run=0x{s['run']:08X} Flash=0x{s['flash']:08X} Size={s['size']} bytes")

if len(hdr) != 320:
    errors.append(f"wire header is {len(hdr)} bytes, expected 320")
if len(segments) != len(INSTALLER_TARGETS):
    errors.append(f"image has {len(segments)} segments, installer handles {len(INSTALLER_TARGETS)}")

image_targets = [s['flash'] for s in segments]
image_runs = [s['run'] for s in segments]

for target in INSTALLER_TARGETS:
    if target not in image_targets:
        errors.append(f"installer target 0x{target:08X} is not present in the image "
                      f"(image targets: {', '.join(f'0x{t:08X}' for t in image_targets)})")

if ENTRY_ADDR not in image_runs:
    errors.append(f"installer entry 0x{ENTRY_ADDR:08X} is not a segment run address "
                  f"(image runs: {', '.join(f'0x{r:08X}' for r in image_runs)})")

payload = sum(s['size'] for s in segments)
usable = STAGING_BYTES - STAGING_MARGIN
print(f"OTA payload: {payload} bytes, staging: {STAGING_BYTES} bytes (usable {usable})")
if payload > usable:
    errors.append(f"OTA payload {payload} B does not fit into staging ({usable} B usable)")

if errors:
    print("\n[FAIL]")
    for e in errors:
        print("  - " + e)
    sys.exit(1)

print("\n[PASS] Segment layout matches the in-app OTA installer and fits the staging area.")
