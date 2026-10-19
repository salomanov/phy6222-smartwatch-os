#include "ota_inapp.h"
#include "ota_service.h"
#include "flash.h"
#include "crc16.h"
#include "log.h"
#include "mcu.h"
#include "vibrator.h"
#include "display.h"
#include "gfx.h"
#include <string.h>

#define COLOR_NAVY 0x000F

#define MAX_OTA_SECTORS 8
#define OTA_BURST_CNT   16
#define OTA_TEMP_BASE   0x11040000 // In-App Staging buffer in Flash (256KB available)

typedef struct {
    uint32_t flash_addr;
    uint32_t run_addr;
    uint32_t size;
    uint16_t checksum;
} ota_inapp_part_t;

static bool s_ota_active = false;
static uint8_t s_ota_progress = 0;
static uint8_t s_sector_count = 0;
static uint8_t s_current_sector = 0;
static uint32_t s_sector_offset = 0;
static uint32_t s_total_fw_size = 0;
static uint32_t s_total_received = 0;
static uint32_t s_burst_accum = 0;
static uint16_t s_running_crc = 0;
static bool s_all_crc_ok = true; // P2-4: tracks if all partition CRCs match

static ota_inapp_part_t s_parts[MAX_OTA_SECTORS];

static void ota_install_firmware_and_reboot(void);

static void send_response(uint8_t cmd_rsp, uint8_t status) {
    attHandleValueNoti_t noti;
    noti.len = 2;
    noti.value[0] = status;
    noti.value[1] = cmd_rsp;
    ota_Notify(&noti);
}

static void draw_ota_progress(uint8_t pct) {
    static bool s_ota_bg_drawn = false;
    if (!s_ota_bg_drawn) {
        s_ota_bg_drawn = true;
        display_fill_screen(COLOR_BLACK);
        gfx_draw_rect(0, 0, 80, 16, COLOR_NAVY);
        gfx_draw_string(8, 4, "OTA FLASH", COLOR_CYAN, COLOR_NAVY, 1);
        gfx_draw_string(8, 115, "DO NOT DISCONNECT", COLOR_RED, COLOR_BLACK, 1);
    }
    
    char pct_str[16];
    pct_str[0] = '0' + (pct / 100);
    pct_str[1] = '0' + ((pct / 10) % 10);
    pct_str[2] = '0' + (pct % 10);
    pct_str[3] = '%';
    pct_str[4] = '\0';
    if (pct_str[0] == '0') {
        pct_str[0] = ' ';
        if (pct_str[1] == '0') pct_str[1] = ' ';
    }
    
    // Fast partial draw only
    gfx_draw_string(14, 50, pct_str, COLOR_YELLOW, COLOR_BLACK, 2);
    gfx_draw_progress_bar(5, 85, 70, 12, pct, COLOR_GREEN, COLOR_DARKGREY);
    
    if (pct >= 100) {
        gfx_draw_string(8, 115, "                  ", COLOR_BLACK, COLOR_BLACK, 1);
        gfx_draw_string(8, 115, "REBOOTING...", COLOR_GREEN, COLOR_BLACK, 1);
        s_ota_bg_drawn = false; // reset for next session
    }
}

bool ota_inapp_is_active(void) {
    return s_ota_active;
}

uint8_t ota_inapp_get_progress(void) {
    return s_ota_progress;
}

static void on_ota_service_event(ota_Evt_t *pev) {
    if (pev->ev == OTA_EVT_CONTROL) {
        uint8_t cmd = pev->data[0];
        LOG("[OTA-INAPP] CMD=0x%02X len=%d\n", cmd, pev->size);
        
        if (cmd == 0x01) { // OTA_CMD_START_OTA: [cmd, sector_num, burst_size]
            s_sector_count = pev->data[1];
            if (s_sector_count > MAX_OTA_SECTORS) {
                send_response(0x81, 0xFF);
                return;
            }
            s_ota_active = true;
            s_ota_progress = 0;
            s_current_sector = 0;
            s_sector_offset = 0;
            s_total_received = 0;
            s_burst_accum = 0;
            s_all_crc_ok = true;
            
            if (pev->size >= 7) {
                s_total_fw_size = pev->data[3] | (pev->data[4]<<8) | (pev->data[5]<<16) | (pev->data[6]<<24);
            } else {
                s_total_fw_size = 0;
            }
            
            // Clean staging area in Flash
            LOG("[OTA-INAPP] Erasing Flash staging area (0x%08X), total size=%d...\n", OTA_TEMP_BASE, s_total_fw_size);
            draw_ota_progress(0);
            vibrator_stop();
            
            // Erase 160KB for incoming firmware
            for (uint32_t a = OTA_TEMP_BASE; a < OTA_TEMP_BASE + 0x28000; a += 0x1000) {
                hal_flash_erase_sector(a);
            }
            
            send_response(0x81, 0x00); // 0x81 = OTA_RSP_START_OTA, SUCCESS
        }
        else if (cmd == 0x02) { // OTA_CMD_PARTITION_INFO
            // [cmd, index, flash_addr(4), run_addr(4), size(4), checksum(2)]
            if (pev->size < 16) {
                send_response(0x84, 0xFF);
                return;
            }
            uint8_t idx = pev->data[1];
            if (idx >= MAX_OTA_SECTORS) {
                send_response(0x84, 0xFF);
                return;
            }
            
            uint32_t flash_addr = pev->data[2] | (pev->data[3]<<8) | (pev->data[4]<<16) | (pev->data[5]<<24);
            uint32_t run_addr = pev->data[6] | (pev->data[7]<<8) | (pev->data[8]<<16) | (pev->data[9]<<24);
            uint32_t size = pev->data[10] | (pev->data[11]<<8) | (pev->data[12]<<16) | (pev->data[13]<<24);
            uint16_t chk = pev->data[14] | (pev->data[15]<<8);
            
            s_parts[idx].flash_addr = flash_addr;
            s_parts[idx].run_addr = run_addr;
            s_parts[idx].size = size;
            s_parts[idx].checksum = chk;
            s_current_sector = idx;
            s_sector_offset = 0;
            s_running_crc = 0;
            if (s_total_fw_size == 0) {
                s_total_fw_size += size;
            }
            
            LOG("[OTA-INAPP] Part %d: Flash=0x%06X Run=0x%08X Size=%d CRC=0x%04X\n",
                idx, flash_addr, run_addr, size, chk);
            
            send_response(0x84, 0x00); // OTA_RSP_PARTITION_INFO, SUCCESS
        }
        else if (cmd == 0x04) { // OTA_CMD_REBOOT
            LOG("[OTA-INAPP] Reboot requested. Finalizing and installing firmware...\n");
            draw_ota_progress(100);
            vibrator_stop();
            send_response(0x8A, 0x00); // OTA_RSP_REBOOT, SUCCESS
            WaitMs(150);
            ota_install_firmware_and_reboot();
        }
        else {
            send_response(cmd | 0x80, 0x00);
        }
    }
    else if (pev->ev == OTA_EVT_DATA) {
        if (!s_ota_active) return;
        
        ota_inapp_part_t *p = &s_parts[s_current_sector];
        uint32_t dest = OTA_TEMP_BASE + s_total_received;
        
        // Write data chunk directly to flash staging area
        hal_flash_write(dest, pev->data, pev->size);
        
        s_running_crc = crc16(s_running_crc, pev->data, pev->size);
        s_sector_offset += pev->size;
        s_total_received += pev->size;
        s_burst_accum += pev->size;
        
        // Calculate progress percentage
        if (s_total_fw_size > 0) {
            uint8_t new_pct = (s_total_received * 100) / s_total_fw_size;
            if (new_pct > 100) new_pct = 100;
            if (new_pct >= s_ota_progress + 5 || new_pct == 100) {
                s_ota_progress = new_pct;
                draw_ota_progress(s_ota_progress);
            }
        }
        
        // Every burst interval send ACK
        if (s_burst_accum >= (240 * OTA_BURST_CNT)) {
            s_burst_accum = 0;
            send_response(0x87, 0x00); // OTA_RSP_BLOCK_BURST
        }
        
        // End of this sector?
        if (s_sector_offset >= p->size) {
            LOG("[OTA-INAPP] Part %d complete! CRC cal=0x%04X exp=0x%04X\n",
                s_current_sector, s_running_crc, p->checksum);
            
            // P2-4: Verify CRC matches expected checksum from partition header
            if (s_running_crc != p->checksum) {
                LOG("[OTA-INAPP] *** CRC MISMATCH on Part %d! Will NOT install. ***\n",
                    s_current_sector);
                s_all_crc_ok = false;
            }
            
            if (s_current_sector + 1 >= s_sector_count) {
                LOG("[OTA-INAPP] ALL SECTORS RECEIVED! Final size=%d\n", s_total_received);
                if (!s_all_crc_ok) {
                    LOG("[OTA-INAPP] *** ABORTING: CRC mismatch detected. Boot tables NOT written. ***\n");
                    send_response(0x83, 0x04); // OTA_RSP_OTA_COMPLETE with CRC error status
                } else {
                    send_response(0x83, 0x00); // OTA_RSP_OTA_COMPLETE success
                }
            } else {
                send_response(0x85, 0x00); // OTA_RSP_PARTITION_COMPLETE
            }
        }
    }
    else if (pev->ev == OTA_EVT_DISCONNECTED) {
        if (s_ota_active && s_ota_progress >= 99 && s_all_crc_ok) {
            LOG("[OTA-INAPP] Disconnected after finish. CRC OK. Installing firmware and reset!\n");
            ota_install_firmware_and_reboot();
        } else if (s_ota_active && s_ota_progress >= 99 && !s_all_crc_ok) {
            LOG("[OTA-INAPP] Disconnected but CRC failed. Staying on current firmware.\n");
        }
        s_ota_active = false;
    }
}

__attribute__((section("_section_sram_code_"), noinline))
static void ota_install_firmware_and_reboot(void) {
    // 0. Disable SysTick and all NVIC interrupt channels so NO ISR can fire
    // even when vendor hal_flash driver executes drv_enable_irq() (cpsie i)
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;
    NVIC->ICER[0] = 0xFFFFFFFF;
    NVIC->ICPR[0] = 0xFFFFFFFF;
    __disable_irq();

    // 1. Erase Bootloader Header sector (0x11002000)
    hal_flash_erase_sector(0x11002000);

    // 2. Erase 2nd bootinfo sector (0x11003000)
    hal_flash_erase_sector(0x11003000);

    // 3. Erase sectors for SRAM data sections (0x11005000 to 0x1100B000)
    for (uint32_t a = 0x11005000; a < 0x1100B000; a += 0x1000) {
        hal_flash_erase_sector(a);
    }

    // 4. Erase target XIP application sectors (0x11010000 to 0x11035000)
    for (uint32_t a = 0x11010000; a < 0x11035000; a += 0x1000) {
        hal_flash_erase_sector(a);
    }

    // 5. Copy 4 partitions from staging buffer (0x11040000) to exact Flash destinations.
    // NOTE: MUST compute target addresses purely with math in CPU registers,
    // NEVER from a const array in Flash, because Flash 0x11010000-0x11035000 is now erased!
    uint8_t buf[256];
    uint32_t staging_offset = 0;

    for (uint8_t i = 0; i < 4 && i < s_sector_count; i++) {
        uint32_t target_flash = 0x11005400; // default Part 3
        if (i == 0) {
            target_flash = 0x11010100;
        } else if (i == 1) {
            target_flash = 0x11020000;
        } else if (i == 2) {
            target_flash = 0x11005000;
        }

        uint32_t size = s_parts[i].size;
        uint32_t copied = 0;

        while (copied < size) {
            uint32_t chunk = size - copied;
            if (chunk > 256) chunk = 256;

            hal_flash_read(OTA_TEMP_BASE + staging_offset + copied, buf, chunk);
            hal_flash_write(target_flash + copied, buf, chunk);

            copied += chunk;
        }

        staging_offset += size;
    }

    // 6. Write Bootloader Header at 0x11002000 (exact wire format for PHY6222 ROM loader)
    // Clear buffer in SRAM loop with volatile pointer to prevent GCC calling libc memset
    volatile uint32_t *p32 = (volatile uint32_t *)buf;
    for (int k = 0; k < 64; k++) {
        p32[k] = 0xFFFFFFFF;
    }

    p32[0] = 4;          // Total segments count
    p32[1] = 0xFFFFFFFF;
    p32[2] = 0x1FFF1838; // Entry start execution address
    p32[3] = 0xFFFFFFFF;
    hal_flash_write(0x11002000, buf, 256);

    // Segments table (at 0x11002100, 4 entries x 16 bytes = 64 bytes)
    for (int k = 0; k < 64; k++) {
        p32[k] = 0xFFFFFFFF;
    }

    // Part 0: XIP 1
    p32[0]  = 0x00010100;
    p32[1]  = s_parts[0].size;
    p32[2]  = 0x11010100;
    p32[3]  = 0xFFFFFFFF;

    // Part 1: XIP 2
    p32[4]  = 0x00020000;
    p32[5]  = s_parts[1].size;
    p32[6]  = 0x11020000;
    p32[7]  = 0xFFFFFFFF;

    // Part 2: SRAM 1
    p32[8]  = 0x00005000;
    p32[9]  = s_parts[2].size;
    p32[10] = 0x1FFF0000;
    p32[11] = 0xFFFFFFFF;

    // Part 3: SRAM 2 (Entry)
    p32[12] = 0x00005400;
    p32[13] = s_parts[3].size;
    p32[14] = 0x1FFF1838;
    p32[15] = 0xFFFFFFFF;

    hal_flash_write(0x11002100, buf, 64);

    // 7. Write 2nd Boot Info Table at 0x11003000 (Required by PHY6222 slboot secondary bootloader!)
    for (int k = 0; k < 64; k++) {
        p32[k] = 0xFFFFFFFF;
    }

    p32[0] = 2;          // ram_part_idx count
    p32[1] = 0;          // Single bank mode (OTAF_SINGLE_BANK)
    p32[2] = 0;
    p32[3] = 0xFFFFFFFF;

    // Part 2 info (SRAM section 1): Flash 0x11005000, Run 0x1FFF0000, Size, Checksum
    p32[4] = 0x11005000;
    p32[5] = 0x1FFF0000;
    p32[6] = s_parts[2].size;
    p32[7] = s_parts[2].checksum;

    // Part 3 info (SRAM section 2 / Entry): Flash 0x11005400, Run 0x1FFF1838, Size, Checksum
    p32[8]  = 0x11005400;
    p32[9]  = 0x1FFF1838;
    p32[10] = s_parts[3].size;
    p32[11] = s_parts[3].checksum;

    hal_flash_write(0x11003000, buf, 48);

    // Write Fast Boot Magic at 0x11003FF0 (bypass slow CRC verification in slboot)
    uint32_t fast_boot = 0xA5A55A5A;
    hal_flash_write(0x11003FF0, (uint8_t *)&fast_boot, 4);

    // 8. Safe Hardware Reset executed strictly from SRAM (no Flash code execution)
    for (volatile int dly = 0; dly < 100000; dly++) {
        __asm__ volatile ("nop");
    }

    AP_AON->SLEEP_R[0] = 4;
    AP_AON->SLEEP_R[1] = 0;
    AP_PCR->SW_RESET1 = 0;

    while (1) {
        __asm__ volatile ("nop");
    }
}

void ota_inapp_init(void) {
    LOG("[OTA-INAPP] Registering In-App OTA service...\n");
    ota_AddService(on_ota_service_event);
}
