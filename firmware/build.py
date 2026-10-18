import os
import sys
import glob
import subprocess
import time

PROJECT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.abspath(os.path.join(PROJECT_DIR, ".."))
REF_DIR = os.path.join(ROOT_DIR, "tools", "smartwatch_ref")
SDK_DIR = os.path.join(REF_DIR, "sdk")
SRC_DIR = os.path.join(PROJECT_DIR, "src")
BUILD_DIR = os.path.join(PROJECT_DIR, "build")

# ARM GCC Toolchain Path
GCC_BIN = r"C:\Users\Salomanov\AppData\Local\Programs\arm-toolchain\xpack-arm-none-eabi-gcc-13.2.1-1.1\bin"
CC = os.path.join(GCC_BIN, "arm-none-eabi-gcc.exe")
OBJCOPY = os.path.join(GCC_BIN, "arm-none-eabi-objcopy.exe")
SIZE = os.path.join(GCC_BIN, "arm-none-eabi-size.exe")

if not os.path.exists(SDK_DIR):
    print(f"[!] ERROR: Vendor SDK directory not found at: {SDK_DIR}")
    print("[!] Clean clone requires the SDK submodule/files. See docs/SDK_SETUP.md for instructions.")
    sys.exit(1)

if not os.path.exists(CC):
    print(f"[!] ERROR: Compiler not found at: {CC}")
    sys.exit(1)

os.makedirs(BUILD_DIR, exist_ok=True)

INCLUDES = [
    SRC_DIR,
    os.path.join(SDK_DIR, "misc"),
    os.path.join(SDK_DIR, "misc", "CMSIS", "include"),
    os.path.join(SDK_DIR, "misc", "CMSIS", "device", "phyplus"),
    os.path.join(SDK_DIR, "components", "arch", "cm0"),
    os.path.join(SDK_DIR, "components", "inc"),
    os.path.join(SDK_DIR, "components", "osal", "include"),
    os.path.join(SDK_DIR, "components", "common"),
    os.path.join(SDK_DIR, "components", "ble", "controller"),
    os.path.join(SDK_DIR, "components", "ble", "include"),
    os.path.join(SDK_DIR, "components", "ble", "hci"),
    os.path.join(SDK_DIR, "components", "ble", "host"),
    os.path.join(SDK_DIR, "components", "profiles", "ota_app"),
    os.path.join(SDK_DIR, "components", "profiles", "ota"),
    os.path.join(SDK_DIR, "components", "profiles", "DevInfo"),
    os.path.join(SDK_DIR, "components", "profiles", "SimpleProfile"),
    os.path.join(SDK_DIR, "components", "profiles", "Roles"),
    os.path.join(SDK_DIR, "components", "driver", "adc"),
    os.path.join(SDK_DIR, "components", "driver", "bsp_button"),
    os.path.join(SDK_DIR, "components", "driver", "clock"),
    os.path.join(SDK_DIR, "components", "driver", "dma"),
    os.path.join(SDK_DIR, "components", "driver", "flash"),
    os.path.join(SDK_DIR, "components", "driver", "gpio"),
    os.path.join(SDK_DIR, "components", "driver", "spi"),
    os.path.join(SDK_DIR, "components", "driver", "i2c"),
    os.path.join(SDK_DIR, "components", "driver", "key"),
    os.path.join(SDK_DIR, "components", "driver", "kscan"),
    os.path.join(SDK_DIR, "components", "driver", "led_light"),
    os.path.join(SDK_DIR, "components", "driver", "log"),
    os.path.join(SDK_DIR, "components", "driver", "pwm"),
    os.path.join(SDK_DIR, "components", "driver", "pwrmgr"),
    os.path.join(SDK_DIR, "components", "driver", "qdec"),
    os.path.join(SDK_DIR, "components", "driver", "spiflash"),
    os.path.join(SDK_DIR, "components", "driver", "timer"),
    os.path.join(SDK_DIR, "components", "driver", "uart"),
    os.path.join(SDK_DIR, "components", "driver", "voice"),
    os.path.join(SDK_DIR, "components", "driver", "watchdog"),
    os.path.join(SDK_DIR, "components", "libraries", "crc16"),
    os.path.join(SDK_DIR, "components", "libraries", "cliface"),
]
INC_FLAGS = [f"-I{d}" for d in INCLUDES]

DEFINES = [
    "-D__GCC",
    "-DDEVICE=DEVICE_WATCH_1",
    "-DCFG_SLEEP_MODE=PWR_MODE_NO_SLEEP",
    "-DDEBUG_INFO=1",
    "-DMTU_SIZE=240",
    "-DMAX_NUM_LL_CONN=1",
    "-DDEF_GAPBOND_MGR_ENABLE=0",
    "-DTEST_RTC_DELTA=1",
    "-DLL_DEBUG_NONE=1",
    "-DCLK_16M_ONLY=1",
    "-DSTACK_MAX_SRAM=1",
    "-DBROADCASTER_CFG=0x01",
    "-DOBSERVER_CFG=0x02",
    "-DPERIPHERAL_CFG=0x04",
    "-DCENTRAL_CFG=0x08",
    "-DHOST_CONFIG=0x04",
    "-DARMCM0",
    "-DOSAL_CBTIMER_NUM_TASKS=1",
    "-DENABLE_LOG_ROM_=0",
    "-DOSALMEM_METRICS=0",
    "-DPHY_MCU_TYPE=MCU_BUMBEE_M0",
    "-DUSE_FS=0",
    "-DOTA_TYPE=OTA_TYPE_NONE",
    "-DDEFAULT_DESIRED_MIN_CONN_INTERVAL=24",
    "-DDEFAULT_DESIRED_MAX_CONN_INTERVAL=80",
    "-DDEFAULT_DESIRED_SLAVE_LATENCY=0",
    "-DDEFAULT_DESIRED_CONN_TIMEOUT=1000",
]

CFLAGS = [
    "-mcpu=cortex-m0", "-mthumb", "-mthumb-interwork",
    "-Os", "-g3", "-ggdb",
    "-W", "-Wall", "--std=gnu99",
    "-fno-diagnostics-show-caret",
    "-fdata-sections", "-ffunction-sections",
    "-funsigned-char", "-funsigned-bitfields",
    "-fms-extensions",
    "-specs=nosys.specs",
] + DEFINES + INC_FLAGS

ASFLAGS = [
    "-mcpu=cortex-m0", "-mthumb", "-mthumb-interwork",
    "-Os", "-g3", "-ggdb",
] + DEFINES + INC_FLAGS

def build():
    t0 = time.time()
    print("====================================================================")
    print("[*] BUILDING CUSTOM SMARTWATCH FIRMWARE (PHY6222)")
    print("====================================================================")

    # Project sources
    src_files = glob.glob(os.path.join(SRC_DIR, "*.c"))

    # SDK sources
    sdk_files = [
        os.path.join(SDK_DIR, "lib", "rf", "patch.c"),
        os.path.join(SDK_DIR, "lib", "sec", "phy_sec_ext.c"),
        os.path.join(SDK_DIR, "lib", "sec", "aes.c"),
        os.path.join(SDK_DIR, "lib", "ble_controller", "rf_phy_driver.c"),
        os.path.join(SDK_DIR, "components", "driver", "clock", "clock.c"),
        os.path.join(SDK_DIR, "components", "driver", "flash", "flash.c"),
        os.path.join(SDK_DIR, "components", "driver", "gpio", "gpio.c"),
        os.path.join(SDK_DIR, "components", "driver", "spi", "spi.c"),
        os.path.join(SDK_DIR, "components", "driver", "dma", "dma.c"),
        os.path.join(SDK_DIR, "components", "driver", "pwrmgr", "pwrmgr.c"),
        os.path.join(SDK_DIR, "components", "driver", "timer", "timer.c"),
        os.path.join(SDK_DIR, "components", "driver", "uart", "uart.c"),
        os.path.join(SDK_DIR, "components", "driver", "watchdog", "watchdog.c"),
        os.path.join(SDK_DIR, "components", "driver", "adc", "adc.c"),
        os.path.join(SDK_DIR, "components", "driver", "log", "my_printf.c"),
        os.path.join(SDK_DIR, "misc", "jump_table.c"),
        os.path.join(SDK_DIR, "misc", "CMSIS", "device", "phyplus", "phy6222_cstart.c"),
        os.path.join(SDK_DIR, "misc", "CMSIS", "device", "phyplus", "phy6222_vectors.c"),

        # BLE Profiles
        os.path.join(SDK_DIR, "components", "profiles", "Roles", "peripheral.c"),
        os.path.join(SDK_DIR, "components", "profiles", "Roles", "gap.c"),
        os.path.join(SDK_DIR, "components", "profiles", "Roles", "gapgattserver.c"),
        os.path.join(SDK_DIR, "components", "profiles", "GATT", "gattservapp.c"),
        os.path.join(SDK_DIR, "components", "profiles", "DevInfo", "devinfoservice.c"),
        os.path.join(SDK_DIR, "components", "profiles", "SimpleProfile", "simpleGATTprofile_ota.c"),
        os.path.join(SDK_DIR, "components", "profiles", "ota", "ota_service.c"),
        os.path.join(SDK_DIR, "components", "libraries", "crc16", "crc16.c"),
    ]

    # BLE Host Library Sources
    ble_host_files = glob.glob(os.path.join(SDK_DIR, "lib", "ble_host", "*.c"))
    sdk_files.extend(ble_host_files)

    asm_file = os.path.join(SDK_DIR, "misc", "CMSIS", "device", "phyplus", "phy6222_start.s")

    objs = []

    # Compile ASM
    asm_obj = os.path.join(BUILD_DIR, "phy6222_start.o")
    print(f"[AS] {os.path.basename(asm_file)}")
    res = subprocess.run([CC] + ASFLAGS + ["-c", asm_file, "-o", asm_obj], capture_output=True, text=True)
    if res.returncode != 0:
        print("[!] ASM ERROR:", res.stderr)
        return False
    objs.append(asm_obj)

    # Compile C
    all_c = src_files + sdk_files
    for f in all_c:
        name = os.path.splitext(os.path.basename(f))[0]
        # Avoid obj collision
        prefix = "src_" if f in src_files else "sdk_"
        obj = os.path.join(BUILD_DIR, f"{prefix}{name}.o")
        print(f"[CC] {os.path.basename(f)}")
        res = subprocess.run([CC] + CFLAGS + ["-c", f, "-o", obj], capture_output=True, text=True)
        if res.returncode != 0:
            print(f"[!] COMPILE ERROR in {f}:\n{res.stderr}")
            return False
        objs.append(obj)

    # Link
    elf = os.path.join(BUILD_DIR, "custom_firmware.elf")
    hex_f = os.path.join(BUILD_DIR, "custom_firmware.hex")
    map_f = os.path.join(BUILD_DIR, "custom_firmware.map")
    ldscript = os.path.join(SDK_DIR, "misc", "phy6222.ld")
    rom_sym = os.path.join(SDK_DIR, "misc", "bb_rom_sym_m0.gcc")

    ldflags = [
        "-mcpu=cortex-m0", "-mthumb", "-mthumb-interwork",
        "--static", "-nostartfiles", "-nostdlib",
        "-Wl,--gc-sections",
        f"-Wl,--script={ldscript}",
        f"-Wl,--just-symbols={rom_sym}",
        f"-Wl,-Map={map_f}",
    ]
    libs = ["-Wl,--start-group", "-lgcc", "-lc", "-lm", "-lnosys", "-Wl,--end-group"]

    print("[LD] Linking custom_firmware.elf...")
    res = subprocess.run([CC] + ldflags + objs + libs + ["-o", elf], capture_output=True, text=True)
    if res.returncode != 0:
        print("[!] LINK ERROR:\n", res.stderr)
        return False

    print("[OBJCOPY] Generating custom_firmware.hex...")
    res = subprocess.run([OBJCOPY, "-O", "ihex", elf, hex_f], capture_output=True, text=True)
    if res.returncode != 0:
        print("[!] OBJCOPY ERROR:\n", res.stderr)
        return False

    # Note: Flat binary (.bin) generation via objcopy is intentionally omitted.
    # PHY6222 memory layout has discontinuous address spaces (XIP Flash 0x11010100 and SRAM 0x1FFF0000).
    # Generating a single flat binary causes objcopy to fill the gap with zeros, producing a useless 240 MB file.
    # Flashing tools use custom_firmware.hex or custom_firmware_ota.bin directly.
    bin_f = os.path.join(BUILD_DIR, "custom_firmware.bin")
    if os.path.exists(bin_f):
        try:
            os.remove(bin_f)
        except OSError:
            pass

    ota_bin = os.path.join(BUILD_DIR, "custom_firmware_ota.bin")
    ota_script = os.path.join(REF_DIR, "phy62x2_ota.py")
    if os.path.exists(ota_script):
        print("[OTA] Generating custom_firmware_ota.bin...")
        res = subprocess.run([sys.executable, ota_script, hex_f, "-o", ota_bin], capture_output=True, text=True)
        if res.returncode == 0:
            print(f"[+] OTA Image: {ota_bin} ({os.path.getsize(ota_bin)} bytes)")
        else:
            print("[!] OTA Pack warning:\n", res.stdout, res.stderr)

    print("--------------------------------------------------------------------")
    subprocess.run([SIZE, "-t", elf])
    dt = time.time() - t0
    print(f"[+] BUILD SUCCESS! in {dt:.2f}s")
    print(f"[+] Output HEX: {hex_f}")
    if os.path.exists(ota_bin):
        print(f"[+] Output OTA: {ota_bin}")
    return True

if __name__ == "__main__":
    success = build()
    sys.exit(0 if success else 1)

