import subprocess

with open('.backups/factory_dump.bin', 'rb') as f:
    f.seek(0x026490)
    code = f.read(0x40)

with open('temp_disasm13.bin', 'wb') as f:
    f.write(code)

cmd = [
    r"C:\Users\Salomanov\AppData\Local\Programs\arm-toolchain\xpack-arm-none-eabi-gcc-13.2.1-1.1\bin\arm-none-eabi-objdump.exe",
    "-D", "-b", "binary", "-m", "arm", "-M", "force-thumb",
    "--adjust-vma=0x11026490",
    "temp_disasm13.bin"
]
res = subprocess.run(cmd, capture_output=True, text=True)
print(res.stdout)
