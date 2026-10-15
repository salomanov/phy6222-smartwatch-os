import struct

with open('.backups/factory_dump.bin', 'rb') as f:
    data = f.read()

# hal_gpio_fmux_set is at jump table or function in ROM/Flash.
# But fmux register on PHY6222 is in AP_IOMUX!
# Let's search where FMUX is configured or search for I2C pin initialization in factory dump:
# Let's search for any function that configures I2C pins.
# In disasm8, line 0x1102645c: bl 0x110260ac
# Let's disassemble 0x110260ac!

import subprocess
with open('.backups/factory_dump.bin', 'rb') as f:
    f.seek(0x0260a0)
    code = f.read(0x80)

with open('temp_disasm10.bin', 'wb') as f:
    f.write(code)

cmd = [
    r"C:\Users\Salomanov\AppData\Local\Programs\arm-toolchain\xpack-arm-none-eabi-gcc-13.2.1-1.1\bin\arm-none-eabi-objdump.exe",
    "-D", "-b", "binary", "-m", "arm", "-M", "force-thumb",
    "--adjust-vma=0x110260a0",
    "temp_disasm10.bin"
]
res = subprocess.run(cmd, capture_output=True, text=True)
print(res.stdout)
