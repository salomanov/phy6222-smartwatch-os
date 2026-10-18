import subprocess

with open('.backups/factory_dump.bin', 'rb') as f:
    f.seek(0x034980)
    code = f.read(0x180)

with open('temp_disasm.bin', 'wb') as f:
    f.write(code)

cmd = [
    r"C:\Users\Salomanov\AppData\Local\Programs\arm-toolchain\xpack-arm-none-eabi-gcc-13.2.1-1.1\bin\arm-none-eabi-objdump.exe",
    "-D", "-b", "binary", "-m", "arm", "-M", "force-thumb",
    "--adjust-vma=0x11034980",
    "temp_disasm.bin"
]
res = subprocess.run(cmd, capture_output=True, text=True)
print(res.stdout)
