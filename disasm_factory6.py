import subprocess

with open('.backups/factory_dump.bin', 'rb') as f:
    f.seek(0x020aa0)
    code = f.read(0x200)

with open('temp_disasm6.bin', 'wb') as f:
    f.write(code)

cmd = [
    r"C:\Users\Salomanov\AppData\Local\Programs\arm-toolchain\xpack-arm-none-eabi-gcc-13.2.1-1.1\bin\arm-none-eabi-objdump.exe",
    "-D", "-b", "binary", "-m", "arm", "-M", "force-thumb",
    "--adjust-vma=0x11020aa0",
    "temp_disasm6.bin"
]
res = subprocess.run(cmd, capture_output=True, text=True)
print(res.stdout)
