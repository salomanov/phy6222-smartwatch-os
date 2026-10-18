import subprocess

with open('.backups/factory_dump.bin', 'rb') as f:
    f.seek(0x0255c8)
    code = f.read(0x30)

with open('temp_disasm_p2.bin', 'wb') as f:
    f.write(code)

cmd = [
    r"C:\Users\Salomanov\AppData\Local\Programs\arm-toolchain\xpack-arm-none-eabi-gcc-13.2.1-1.1\bin\arm-none-eabi-objdump.exe",
    "-D", "-b", "binary", "-m", "arm", "-M", "force-thumb",
    "--adjust-vma=0x110255c8",
    "temp_disasm_p2.bin"
]
res = subprocess.run(cmd, capture_output=True, text=True)
print(res.stdout)
