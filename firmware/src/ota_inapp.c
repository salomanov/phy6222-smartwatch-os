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
    display_fill_screen(COLOR_BLACK);
    gfx_draw_rect(0, 0, 80, 16, COLOR_NAVY);
    gfx_draw_string(8, 4, "OTA FLASH", COLOR_CYAN, COLOR_NAVY, 1);
    
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
    
    gfx_draw_string(14, 50, pct_str, COLOR_YELLOW, COLOR_BLACK, 2);
    gfx_draw_progress_bar(5, 85, 70, 12, pct, COLOR_GREEN, COLOR_DARKGREY);
    
    if (pct < 100) {
        gfx_draw_string(8, 115, "DO NOT DISCONNECT", COLOR_RED, COLOR_BLACK, 1);
    } else {
        gfx_draw_string(12, 115, "REBOOTING...", COLOR_GREEN, COLOR_BLACK, 1);
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
            s_total_fw_size = 0;
            s_total_received = 0;
            s_burst_accum = 0;
            
            // Clean staging area in Flash
            LOG("[OTA-INAPP] Erasing Flash staging area (0x%08X)...\n", OTA_TEMP_BASE);
            draw_ota_progress(0);
            vibrator_pulse(100);
            
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
            s_total_fw_size += size;
            
            LOG("[OTA-INAPP] Part %d: Flash=0x%06X Run=0x%08X Size=%d CRC=0x%04X\n",
                idx, flash_addr, run_addr, size, chk);
            
            send_response(0x84, 0x00); // OTA_RSP_PARTITION_INFO, SUCCESS
        }
        else if (cmd == 0x04) { // OTA_CMD_REBOOT
            LOG("[OTA-INAPP] Reboot requested. Finalizing and installing firmware...\n");
            draw_ota_progress(100);
            vibrator_pulse(200);
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
            if (new_pct != s_ota_progress) {
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
            
            if (s_current_sector + 1 >= s_sector_count) {
                LOG("[OTA-INAPP] ALL SECTORS RECEIVED! Final size=%d\n", s_total_received);
                send_response(0x83, 0x00); // OTA_RSP_OTA_COMPLETE
            } else {
                send_response(0x85, 0x00); // OTA_RSP_PARTITION_COMPLETE
            }
        }
    }
    else if (pev->ev == OTA_EVT_DISCONNECTED) {
        if (s_ota_active && s_ota_progress >= 99) {
            LOG("[OTA-INAPP] Disconnected after finish. Installing firmware and reset!\n");
            ota_install_firmware_and_reboot();
        }
        s_ota_active = false;
    }
}

__attribute__((section("_section_sram_code_"), noinline))
static void ota_install_firmware_and_reboot(void) {
    __disable_irq();

    // 1. Erase target execution flash sectors (0x11010000 to 0x11035000)
    for (uint32_t a = 0x11010000; a < 0x11035000; a += 0x1000) {
        hal_flash_erase_sector(a);
    }

    // 2. Copy received partitions from staging area (0x11040000) to target flash
    uint8_t buf[256];
    uint32_t staging_offset = 0;

    for (uint8_t i = 0; i < s_sector_count; i++) {
        uint32_t target_flash = 0x11000000 | (s_parts[i].flash_addr & 0x00FFFFFF);
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

    // 3. Write Bootloader Header at 0x11002000
    hal_flash_erase_sector(0x11002000);

    memset(buf, 0xFF, 256);
    uint32_t *hdr32 = (uint32_t *)buf;
    hdr32[0] = s_sector_count; // Number of segments
    hdr32[1] = 0xFFFFFFFF;
    hdr32[2] = 0x1FFF1838;     // Start execution address
    hdr32[3] = 0xFFFFFFFF;
    hal_flash_write(0x11002000, buf, 256);

    memset(buf, 0xFF, 256);
    for (uint8_t i = 0; i < s_sector_count; i++) {
        uint32_t *p32 = (uint32_t *)&buf[i * 16];
        // If partition runs in XIP Flash (0x11xxxxxx), flash_addr must be 0x11xxxxxx
        if (s_parts[i].run_addr >= 0x11000000) {
            p32[0] = s_parts[i].run_addr;
        } else {
            p32[0] = s_parts[i].flash_addr & 0x00FFFFFF;
        }
        p32[1] = s_parts[i].size;       // Segment size
        p32[2] = s_parts[i].run_addr;   // Run/RAM/XIP address
        p32[3] = 0xFFFFFFFF;           // Flag / reserved
    }
    hal_flash_write(0x11002100, buf, s_sector_count * 16);

    // 4. Also write 2nd bootinfo at 0x11003000
    hal_flash_erase_sector(0x11003000);
    memset(buf, 0xFF, 256);
    hdr32 = (uint32_t *)buf;
    hdr32[0] = s_sector_count;
    hdr32[1] = 0; // single bank
    hdr32[2] = 0;
    hdr32[3] = 0xFFFFFFFF;
    hal_flash_write(0x11003000, buf, 16);
    for (uint8_t i = 0; i < s_sector_count; i++) {
        uint32_t b2_part[4];
        if (s_parts[i].run_addr >= 0x11000000) {
            b2_part[0] = s_parts[i].run_addr;
        } else {
            b2_part[0] = s_parts[i].flash_addr & 0x00FFFFFF;
        }
        b2_part[1] = s_parts[i].run_addr;
        b2_part[2] = s_parts[i].size;
        b2_part[3] = (uint32_t)s_parts[i].checksum;
        hal_flash_write(0x11003010 + i * 16, (uint8_t *)b2_part, 16);
    }

    // 5. Short delay and restart
    for (volatile int dly = 0; dly < 100000; dly++) {
        __asm__ volatile ("nop");
    }

    hal_system_soft_reset();
}

void ota_inapp_init(void) {
    LOG("[OTA-INAPP] Registering In-App OTA service...\n");
    ota_AddService(on_ota_service_event);
}
