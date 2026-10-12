#include "app_os.h"
#include "ota_inapp.h"
#include "display.h"
#include "gfx.h"
#include "button.h"
#include "vibrator.h"
#include "uart_cli.h"
#include "mcu.h"
#include "ble_manager.h"
#include "bcomdef.h"
#include "peripheral.h"
#include "battery.h"
#include <stdio.h>
#include <string.h>

#define COLOR_NAVY 0x000F

static os_mode_t s_current_mode = MODE_WATCHFACE;
static uint32_t s_uptime_seconds = 0;
static uint32_t s_last_sec_tick = 0;
static uint32_t s_ms_counter = 0;

// Watch Time (Simulated RTC starting at 12:34:00)
static uint8_t s_hours = 12;
static uint8_t s_minutes = 34;
static uint8_t s_seconds = 0;

static bool s_needs_redraw = true;
static uint32_t s_click_count = 0;
static uint32_t s_vib_count = 0;
static bool s_diag_first_draw = true;

static void render_watchface(void) {
    if (s_diag_first_draw) {
        s_diag_first_draw = false;
        display_fill_screen(COLOR_BLACK);
        gfx_draw_rect(0, 0, 80, 14, COLOR_NAVY);
        gfx_draw_string(6, 3, "LP715 DIAG", COLOR_CYAN, COLOR_NAVY, 1);
        gfx_draw_fast_h_line(2, 66, 76, COLOR_DARKGREY);
        gfx_draw_string(4, 70, "CHASY-STEALTH", COLOR_CYAN, COLOR_BLACK, 1);
        gfx_draw_string(4, 82, "RF: 1M 0dBm", COLOR_GREEN, COLOR_BLACK, 1);
        gfx_draw_fast_h_line(2, 94, 76, COLOR_DARKGREY);
        gfx_draw_string(4, 98, "PERIPHERALS:", COLOR_YELLOW, COLOR_BLACK, 1);
        gfx_draw_fast_h_line(2, 136, 76, COLOR_DARKGREY);
        gfx_draw_string(4, 142, "CLICK: VIB SCAN", COLOR_GREEN, COLOR_BLACK, 1);
        gfx_draw_string(4, 152, "HOLD: OTA MODE", COLOR_CYAN, COLOR_BLACK, 1);
    }

    // Hardware status: Uptime & Battery
    char buf[20];
    snprintf(buf, sizeof(buf), "UP: %-4lus", (unsigned long)s_uptime_seconds);
    gfx_draw_string(4, 18, buf, COLOR_WHITE, COLOR_BLACK, 1);

    // Battery Line (Ponytail style: clean and concise)
    uint8_t bat_pct = battery_get_percent();
    uint16_t bat_mv = battery_read_mv();
    snprintf(buf, sizeof(buf), "BAT: %d%% %dmV", bat_pct, bat_mv);
    gfx_draw_string(4, 30, buf, (bat_pct > 20) ? COLOR_GREEN : COLOR_RED, COLOR_BLACK, 1);

    // BLE State Line
    uint8_t bst = ble_get_state();
    uint8_t cst = ble_get_cb_state();
    uint8_t bret = ble_get_start_ret();
    uint16_t bcalls = ble_get_start_calls();

    const char *st_name = "INIT";
    uint16_t st_col = COLOR_YELLOW;
    if (bst == 1) { st_name = "START"; st_col = COLOR_CYAN; }
    else if (bst == 2) { st_name = "ADV  "; st_col = COLOR_GREEN; }
    else if (bst == 3) { st_name = "CONN "; st_col = COLOR_MAGENTA; }
    else if (bst >= 4) { st_name = "WAIT "; st_col = COLOR_ORANGE; }

    snprintf(buf, sizeof(buf), "BLE: %-5s", st_name);
    gfx_draw_string(4, 42, buf, st_col, COLOR_BLACK, 1);

    snprintf(buf, sizeof(buf), "B:%d C:%d R:%02X", bst, cst, bret);
    gfx_draw_string(4, 54, buf, COLOR_LIGHTGREY, COLOR_BLACK, 1);

    // Button input counter
    snprintf(buf, sizeof(buf), "BTN: %-4lu evt", (unsigned long)s_click_count);
    gfx_draw_string(4, 110, buf, COLOR_WHITE, COLOR_BLACK, 1);

    // Vibrator pulse counter
    snprintf(buf, sizeof(buf), "VIB: %-4lu buzz", (unsigned long)s_vib_count);
    gfx_draw_string(4, 122, buf, COLOR_WHITE, COLOR_BLACK, 1);
}

static void render_vibrator_test(void) {
    display_fill_screen(COLOR_BLACK);
    gfx_draw_rect(0, 0, 80, 14, COLOR_NAVY);
    gfx_draw_string(4, 3, "VIB SCANNER", COLOR_YELLOW, COLOR_NAVY, 1);

    gfx_draw_fast_h_line(2, 20, 76, COLOR_DARKGREY);
    gfx_draw_string(4, 26, "TEST PIN:", COLOR_WHITE, COLOR_BLACK, 1);

    char p_buf[20];
    snprintf(p_buf, sizeof(p_buf), "%s", vibrator_get_test_pin_name());
    gfx_draw_string(8, 40, p_buf, COLOR_CYAN, COLOR_BLACK, 2);

    snprintf(p_buf, sizeof(p_buf), "POL: %s", vibrator_get_active_level() ? "HIGH (1)" : "LOW (0)");
    gfx_draw_string(4, 62, p_buf, vibrator_get_active_level() ? COLOR_GREEN : COLOR_ORANGE, COLOR_BLACK, 1);

    gfx_draw_fast_h_line(2, 78, 76, COLOR_DARKGREY);
    gfx_draw_string(4, 84, "Click: Next Pin", COLOR_LIGHTGREY, COLOR_BLACK, 1);
    gfx_draw_string(4, 98, "2Click: Polarity", COLOR_LIGHTGREY, COLOR_BLACK, 1);
    gfx_draw_string(4, 112, "Hold: Return Home", COLOR_LIGHTGREY, COLOR_BLACK, 1);

    gfx_draw_fast_h_line(2, 128, 76, COLOR_DARKGREY);
    gfx_draw_string(4, 134, "STATUS: PULSING", COLOR_YELLOW, COLOR_BLACK, 1);
    gfx_draw_string(4, 146, "Impulse 350ms", COLOR_WHITE, COLOR_BLACK, 1);
}

static void render_ota_update(void) {
    display_fill_screen(COLOR_BLACK);
    gfx_draw_rect(0, 0, 80, 14, COLOR_NAVY);
    gfx_draw_string(6, 3, "OTA UPDATE", COLOR_CYAN, COLOR_NAVY, 1);

    gfx_draw_fast_h_line(2, 24, 76, COLOR_DARKGREY);
    gfx_draw_string(8, 30, "WIRELESS", COLOR_YELLOW, COLOR_BLACK, 1);
    gfx_draw_string(8, 42, "FLASH MODE", COLOR_YELLOW, COLOR_BLACK, 1);
    gfx_draw_fast_h_line(2, 56, 76, COLOR_DARKGREY);

    gfx_draw_string(4, 66, "Hold sensor", COLOR_WHITE, COLOR_BLACK, 1);
    gfx_draw_string(4, 78, "to reboot into", COLOR_WHITE, COLOR_BLACK, 1);
    gfx_draw_string(4, 90, "PPlusOTA boot", COLOR_GREEN, COLOR_BLACK, 1);

    gfx_draw_fast_h_line(2, 114, 76, COLOR_DARKGREY);
    gfx_draw_string(4, 122, "Click: Return", COLOR_LIGHTGREY, COLOR_BLACK, 1);
    gfx_draw_string(4, 136, "HOLD: FLASH", COLOR_CYAN, COLOR_BLACK, 1);
}

void app_os_panic(void) {
    s_current_mode = MODE_WATCHFACE;
    s_diag_first_draw = true;
    s_needs_redraw = true;
    vibrator_pulse(40);
}

void app_os_set_time(uint8_t h, uint8_t m, uint8_t s) {
    s_hours = h % 24;
    s_minutes = m % 60;
    s_seconds = s % 60;
    s_needs_redraw = true;
}

void app_os_init(void) {
    display_init(0); // Portrait 80x160
    button_init();
    vibrator_init();
    battery_init();
    uart_cli_init();
    
    // No buzz on startup (safe and quiet)
    s_needs_redraw = false;
    render_watchface();
}

void app_os_update(void) {
    s_ms_counter += 50; // Called every 50ms by OSAL timer (WATCH_TICK_EVT)
    uart_cli_update();

    // 1-second system timer
    if (s_ms_counter - s_last_sec_tick >= 1000) {
        s_last_sec_tick = s_ms_counter;
        s_uptime_seconds++;
        s_seconds++;
        if (s_seconds >= 60) {
            s_seconds = 0;
            s_minutes++;
            if (s_minutes >= 60) {
                s_minutes = 0;
                s_hours = (s_hours + 1) % 24;
            }
        }
        // If BLE is still in INIT (B:0), trigger GAPRole_StartDevice once at startup
        if (ble_get_state() == 0 && ble_get_start_calls() == 0) {
            ble_try_start_device();
        }

        if (s_current_mode == MODE_WATCHFACE) {
            s_needs_redraw = true;
        }
    }

    // Process button events
    button_event_t evt = button_update(s_ms_counter);

    if (evt == BTN_EVENT_CLICK) {
        s_click_count++;
        s_vib_count++;

        if (s_current_mode == MODE_WATCHFACE) {
            // Tactile 80ms haptic tap feedback
            vibrator_pulse(80);
        } else if (s_current_mode == MODE_VIBRATOR_TEST) {
            vibrator_next_test_pin();
            vibrator_test_fire();
        } else if (s_current_mode == MODE_OTA_UPDATE) {
            s_current_mode = MODE_WATCHFACE;
            s_diag_first_draw = true;
        }
        s_needs_redraw = true;
    }

    if (evt == BTN_EVENT_DOUBLE_CLICK) {
        s_click_count += 2;
        s_vib_count += 2;

        if (s_current_mode == MODE_VIBRATOR_TEST) {
            // Double click: toggle active polarity (HIGH vs LOW) and test fire!
            vibrator_toggle_active_level();
            vibrator_test_fire();
        } else {
            // Double click from any screen: return to main watchface
            s_current_mode = MODE_WATCHFACE;
            s_diag_first_draw = true;
            ble_try_start_device();
        }
        s_needs_redraw = true;
    }

    if (evt == BTN_EVENT_LONG_PRESS) {
        s_click_count += 5;
        s_vib_count += 3;

        if (s_current_mode == MODE_OTA_UPDATE) {
            // REBOOT INTO OTA BOOTLOADER
            ble_start_ota_mode();
            return;
        } else if (s_current_mode == MODE_VIBRATOR_TEST) {
            // Long press in test saves and returns home
            vibrator_select_current_pin();
            s_current_mode = MODE_WATCHFACE;
            s_diag_first_draw = true;
        } else if (s_current_mode == MODE_WATCHFACE) {
            // Long press from watchface: OTA MODE!
            s_current_mode = MODE_OTA_UPDATE;
        }
        s_needs_redraw = true;
    }

    // Redraw screen if requested
    if (s_needs_redraw && !ota_inapp_is_active()) {
        s_needs_redraw = false;
        switch (s_current_mode) {
            case MODE_WATCHFACE:
                render_watchface();
                break;
            case MODE_VIBRATOR_TEST:
                render_vibrator_test();
                break;
            case MODE_OTA_UPDATE:
                render_ota_update();
                break;
            default:
                break;
        }
    }
}
