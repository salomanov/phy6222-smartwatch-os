#include "app_os.h"
#include "config.h"
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
#include "accel.h"
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
    uint8_t bat_pct = battery_get_percent();
    uint16_t bat_mv = battery_read_mv();

    if (s_diag_first_draw) {
        s_diag_first_draw = false;
        display_fill_screen(COLOR_BLACK);
        
        // Header (Navy Blue Bar)
        gfx_fill_rect(0, 0, 80, 15, COLOR_NAVY);
        gfx_draw_string(23, 4, "LP715", COLOR_WHITE, COLOR_NAVY, 1);
        
        gfx_draw_fast_h_line(2, 17, 76, COLOR_DARKGREY);
        gfx_draw_fast_h_line(2, 42, 76, COLOR_DARKGREY);
        gfx_draw_fast_h_line(2, 60, 76, COLOR_DARKGREY);
        gfx_draw_fast_h_line(2, 86, 76, COLOR_DARKGREY);
        
        gfx_draw_string(4, 90, "UPTIME:", COLOR_YELLOW, COLOR_BLACK, 1);
        gfx_draw_fast_h_line(2, 110, 76, COLOR_DARKGREY);
        gfx_draw_string(4, 114, FW_VERSION_STR, COLOR_GREEN, COLOR_BLACK, 1);
        gfx_draw_fast_h_line(2, 138, 76, COLOR_DARKGREY);
        gfx_draw_string(4, 144, "HOLD: OTA", COLOR_CYAN, COLOR_BLACK, 1);
    }

    char buf[24];

    // 0. Header Bar Icons (Top right: Battery outline+level; Top left: BLE status)
    gfx_draw_ble_icon(3, 3, ble_is_connected());
    gfx_draw_battery_icon(58, 3, bat_pct, battery_is_charging());

    // 1. Dynamic Clock / Time (Scale 2 for HH:MM, Scale 1 for SS)
    snprintf(buf, sizeof(buf), "%02d:%02d", s_hours, s_minutes);
    gfx_draw_string(6, 22, buf, COLOR_CYAN, COLOR_BLACK, 2); // 5 chars * 12 px = 60 px

    snprintf(buf, sizeof(buf), "%02d", s_seconds);
    gfx_draw_string(66, 30, buf, COLOR_YELLOW, COLOR_BLACK, 1);

    // 2. Activity Row: Heart Rate & Step count with graphical icons
    gfx_draw_heart_icon(4, 47, COLOR_RED);
    gfx_draw_string(14, 48, "--", COLOR_WHITE, COLOR_BLACK, 1);

    gfx_draw_step_icon(42, 47, COLOR_CYAN);
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)s_click_count);
    gfx_draw_string(52, 48, buf, COLOR_WHITE, COLOR_BLACK, 1);

    // 3. Battery status & Mini-gauge
    snprintf(buf, sizeof(buf), "%3d%% %4dmV", bat_pct, bat_mv); // 10 chars * 6 px = 60 px < 80 px
    gfx_draw_string(4, 65, buf, (bat_pct < 20) ? COLOR_RED : COLOR_GREEN, COLOR_BLACK, 1);
    gfx_draw_progress_bar(4, 75, 72, 6, bat_pct, (bat_pct < 20) ? COLOR_RED : COLOR_GREEN, COLOR_DARKGREY);

    // 4. Dynamic ticking Uptime: HH:MM:SS
    uint32_t sec = s_uptime_seconds;
    uint32_t h = sec / 3600;
    uint32_t m = (sec % 3600) / 60;
    uint32_t s = sec % 60;
    snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", (unsigned long)h, (unsigned long)m, (unsigned long)s);
    gfx_draw_string(4, 99, buf, COLOR_WHITE, COLOR_BLACK, 1);

    // 5. BLE status line
    if (ble_is_connected()) {
        gfx_draw_string(4, 126, "BLE: ONLINE", COLOR_GREEN, COLOR_BLACK, 1);
    } else {
        gfx_draw_string(4, 126, "BLE: ADV...", COLOR_YELLOW, COLOR_BLACK, 1);
    }
}

static void render_vibrator_test(void) {
    display_fill_screen(COLOR_BLACK);
    gfx_fill_rect(0, 0, 80, 15, COLOR_NAVY);
    gfx_draw_string(4, 4, "VIB SCANNER", COLOR_YELLOW, COLOR_NAVY, 1);

    gfx_draw_fast_h_line(2, 18, 76, COLOR_DARKGREY);
    gfx_draw_string(4, 24, "TEST PIN:", COLOR_WHITE, COLOR_BLACK, 1);

    char p_buf[20];
    snprintf(p_buf, sizeof(p_buf), "%s", vibrator_get_test_pin_name());
    gfx_draw_string(8, 38, p_buf, COLOR_CYAN, COLOR_BLACK, 2); // max 3 chars * 12 px = 36 px

    // Role hint at scale 1 (the old "P03 (Vib)" name was 9 chars * 12 px = 108 px and wrapped)
    const char *p_tag = vibrator_get_test_pin_tag();
    if (p_tag[0]) {
        gfx_draw_string(52, 44, p_tag, COLOR_LIGHTGREY, COLOR_BLACK, 1);
    }

    snprintf(p_buf, sizeof(p_buf), "POL: %s", vibrator_get_active_level() ? "HIGH (1)" : "LOW (0) ");
    gfx_draw_string(4, 58, p_buf, vibrator_get_active_level() ? COLOR_GREEN : COLOR_ORANGE, COLOR_BLACK, 1);

    gfx_draw_fast_h_line(2, 72, 76, COLOR_DARKGREY);
    gfx_draw_string(4, 78, "Click: Next", COLOR_LIGHTGREY, COLOR_BLACK, 1);
    gfx_draw_string(4, 92, "Dbl: Polar ", COLOR_LIGHTGREY, COLOR_BLACK, 1);
    gfx_draw_string(4, 106, "Hold: Exit ", COLOR_LIGHTGREY, COLOR_BLACK, 1);

    gfx_draw_fast_h_line(2, 120, 76, COLOR_DARKGREY);
    gfx_draw_string(4, 126, "ST: PULSING", COLOR_YELLOW, COLOR_BLACK, 1);
    gfx_draw_string(4, 138, "Pulse 350ms", COLOR_WHITE, COLOR_BLACK, 1);
}

static void render_ota_update(void) {
    display_fill_screen(COLOR_BLACK);
    gfx_fill_rect(0, 0, 80, 15, COLOR_NAVY);
    gfx_draw_string(7, 4, "OTA UPDATE", COLOR_CYAN, COLOR_NAVY, 1);

    gfx_draw_fast_h_line(2, 20, 76, COLOR_DARKGREY);
    gfx_draw_string(8, 28, "WIRELESS", COLOR_YELLOW, COLOR_BLACK, 1);
    gfx_draw_string(8, 40, "FLASH MODE", COLOR_YELLOW, COLOR_BLACK, 1);
    gfx_draw_fast_h_line(2, 54, 76, COLOR_DARKGREY);

    gfx_draw_string(4, 62, "Hold sensor", COLOR_WHITE, COLOR_BLACK, 1);
    gfx_draw_string(4, 74, "to reboot  ", COLOR_WHITE, COLOR_BLACK, 1);
    gfx_draw_string(4, 86, "OTA loader ", COLOR_GREEN, COLOR_BLACK, 1);

    gfx_draw_fast_h_line(2, 108, 76, COLOR_DARKGREY);
    gfx_draw_string(4, 118, "Click: Back", COLOR_LIGHTGREY, COLOR_BLACK, 1);
    gfx_draw_string(4, 134, "HOLD: FLASH", COLOR_CYAN, COLOR_BLACK, 1);
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

void app_os_force_redraw(void) {
    s_diag_first_draw = true;
    s_needs_redraw = false;
    render_watchface();
}

void app_os_init(void) {
    display_init(0); // Portrait 80x160
    button_init();
    vibrator_init();
    battery_init();
    
    // Hardware sensors power lines (from factory ROM reverse engineering)
    hal_gpio_cfg_analog_io(ACCEL_PWR_PIN, Bit_DISABLE);
    hal_gpio_pin_init(ACCEL_PWR_PIN, GPIO_OUTPUT);
    hal_gpio_write(ACCEL_PWR_PIN, 1); // Power ON accelerometer (P15)

    hal_gpio_cfg_analog_io(HR_PWR_PIN, Bit_DISABLE);
    hal_gpio_pin_init(HR_PWR_PIN, GPIO_OUTPUT);
    hal_gpio_write(HR_PWR_PIN, 1); // Power OFF optical HR sensor (P00 active LOW)

    hal_gpio_cfg_analog_io(HR_LED_PIN, Bit_DISABLE);
    hal_gpio_pin_init(HR_LED_PIN, GPIO_OUTPUT);
    hal_gpio_write(HR_LED_PIN, 0); // Green LED OFF (P14 active HIGH)

    uart_cli_init();
    
    // No buzz on startup (safe and quiet)
    s_needs_redraw = false;
    render_watchface();
}

void app_os_update(void) {
    if (ota_inapp_is_active()) {
        vibrator_stop();
        uart_cli_update();
        return;
    }

    s_ms_counter += 50; // Called every 50ms by OSAL timer (WATCH_TICK_EVT)
    vibrator_update(50); // Non-blocking vibrator timer (P2-2: prevents blocking OSAL tick)
    uart_cli_update();

    // 1-second system timer
    if (s_ms_counter - s_last_sec_tick >= 1000) {
        s_last_sec_tick = s_ms_counter;
        s_uptime_seconds++; if ((s_uptime_seconds % 5) == 0) battery_sample_now();
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
            // Tactile 80ms haptic tap feedback (non-blocking)
            vibrator_pulse(80);
            // Note: repeated accel_init() on button click is removed (P2-3, ERRORS_AND_FAILURES 1.6: avoids SPI display bus collisions)
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
