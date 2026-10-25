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
#include "factory_assets.h"
#include <stdio.h>
#include <string.h>

#define COLOR_NAVY 0x000F
#define WF_PAGE_COUNT 6

static os_mode_t s_current_mode = MODE_WATCHFACE;
static uint8_t s_wf_page = 0;
static uint32_t s_uptime_seconds = 0;
static uint32_t s_last_sec_tick = 0;
static uint32_t s_ms_counter = 0;
static uint8_t s_anim_frame = 0;

// Watch Time (Simulated RTC starting at 12:34:00)
static uint8_t s_hours = 12;
static uint8_t s_minutes = 34;
static uint8_t s_seconds = 0;

static bool s_needs_redraw = true;
static uint32_t s_click_count = 0;
static uint32_t s_vib_count = 0;
static bool s_diag_first_draw = true;

// Page 0: Main Watchface (100% preserved)
static void render_watchface_page0(void) {
    uint8_t bat_pct = battery_get_percent();
    uint16_t bat_mv = battery_read_mv();

    if (s_diag_first_draw) {
        s_diag_first_draw = false;
        display_fill_screen(COLOR_BLACK);
        
        // Header (Navy Blue Bar)
        gfx_fill_rect(0, 0, 80, 15, COLOR_NAVY);
        gfx_draw_string(23, 4, "LP715", COLOR_WHITE, COLOR_NAVY, 1);
        
        gfx_draw_fast_h_line(2, 17, 76, COLOR_DARKGREY);
        gfx_draw_fast_h_line(2, 38, 76, COLOR_DARKGREY);
        gfx_draw_fast_h_line(2, 82, 76, COLOR_DARKGREY);
        gfx_draw_fast_h_line(2, 104, 76, COLOR_DARKGREY);
        gfx_draw_fast_h_line(2, 134, 76, COLOR_DARKGREY);
        
        gfx_draw_string(4, 41, "STEPS:", COLOR_YELLOW, COLOR_BLACK, 1);
        gfx_draw_string(4, 120, "v1.5.17-FIX", COLOR_LIGHTGREY, COLOR_BLACK, 1);
        gfx_draw_string(4, 148, "HOLD: OTA", COLOR_CYAN, COLOR_BLACK, 1);
    }

    char buf[24];

    // 0. Header Bar Icons
    gfx_draw_ble_icon(3, 3, ble_is_connected());
    gfx_draw_battery_icon(58, 3, bat_pct, battery_is_charging());

    // 1. Dynamic Clock / Time (Scale 2 for HH:MM, Scale 1 for SS)
    gfx_fill_rect(6, 20, 58, 16, COLOR_BLACK);
    snprintf(buf, sizeof(buf), "%02d:%02d", s_hours, s_minutes);
    gfx_draw_string(6, 20, buf, COLOR_CYAN, COLOR_BLACK, 2);

    gfx_fill_rect(66, 27, 12, 8, COLOR_BLACK);
    snprintf(buf, sizeof(buf), "%02d", s_seconds);
    gfx_draw_string(66, 27, buf, COLOR_YELLOW, COLOR_BLACK, 1);

    // 2. Motion / Steps Row
    gfx_fill_rect(46, 41, 30, 8, COLOR_BLACK);
    snprintf(buf, sizeof(buf), "P18:%d", hal_gpio_read(ACCEL_INT_PIN));
    gfx_draw_string(46, 41, buf, COLOR_LIGHTGREY, COLOR_BLACK, 1);

    // Large Step Counter (Scale 2, max 5 chars = 60 px)
    gfx_fill_rect(10, 52, 60, 16, COLOR_BLACK);
    snprintf(buf, sizeof(buf), "%5lu", (unsigned long)accel_get_steps());
    gfx_draw_string(10, 52, buf, COLOR_GREEN, COLOR_BLACK, 2);

    // Raw pulses (Scale 1)
    gfx_fill_rect(4, 71, 72, 8, COLOR_BLACK);
    snprintf(buf, sizeof(buf), "PULSE:%-5lu", (unsigned long)accel_get_pulses());
    gfx_draw_string(4, 71, buf, COLOR_CYAN, COLOR_BLACK, 1);

    // 3. Battery status & Mini-gauge (Clean row clear prevents overlapping digits/percents)
    gfx_fill_rect(4, 85, 72, 17, COLOR_BLACK);
    snprintf(buf, sizeof(buf), "%3d%% %4dmV", bat_pct, bat_mv);
    gfx_draw_string(4, 85, buf, (bat_pct < 20) ? COLOR_RED : COLOR_GREEN, COLOR_BLACK, 1);
    gfx_draw_progress_bar(4, 95, 72, 5, bat_pct, (bat_pct < 20) ? COLOR_RED : COLOR_GREEN, COLOR_DARKGREY);

    // 4. Dynamic Uptime: UP:HH:MM:SS
    uint32_t sec = s_uptime_seconds;
    uint32_t h = sec / 3600;
    uint32_t m = (sec % 3600) / 60;
    uint32_t s = sec % 60;
    gfx_fill_rect(4, 108, 72, 8, COLOR_BLACK);
    snprintf(buf, sizeof(buf), "UP:%02lu:%02lu:%02lu", (unsigned long)h, (unsigned long)m, (unsigned long)s);
    gfx_draw_string(4, 108, buf, COLOR_WHITE, COLOR_BLACK, 1);

    // 5. BLE status line
    if (ble_is_connected()) {
        gfx_draw_string(4, 137, "BLE:ONLINE", COLOR_GREEN, COLOR_BLACK, 1);
    } else {
        gfx_draw_string(4, 137, "BLE:ADV...", COLOR_YELLOW, COLOR_BLACK, 1);
    }
}

// Page 1: Factory Watchface #1 (0x1107260C 8bpp BMP wallpaper + 0x110721C2 giant digits + ROM icons)
static void render_watchface_page1(void) {
    uint8_t bat_pct = battery_get_percent();
    if (s_diag_first_draw) {
        s_diag_first_draw = false;
        factory_draw_bmp_wallpaper();
    }

    factory_draw_sprite_1bpp(FACTORY_SPRITE_BLE_ADDR, 4, 4,
                             ble_is_connected() ? COLOR_CYAN : COLOR_LIGHTGREY, COLOR_BLACK, true);

    uint8_t bat_idx = bat_pct / 25;
    if (bat_idx > 4) bat_idx = 4;
    factory_draw_sprite_1bpp(FACTORY_SPRITE_BAT_BASE + (uint32_t)bat_idx * 26U, 60, 4,
                             COLOR_GREEN, COLOR_BLACK, true);

    uint8_t wk_idx = (uint8_t)((s_uptime_seconds / 3U) % 7U);
    factory_draw_sprite_1bpp(FACTORY_SPRITE_WKDAY_BASE + (uint32_t)wk_idx * 50U, 24, 18,
                             COLOR_YELLOW, COLOR_BLACK, true);

    factory_draw_big_time(s_hours, s_minutes, 34, COLOR_WHITE, true);

    char buf[16];
    snprintf(buf, sizeof(buf), "25/10-%02d", s_seconds);
    factory_draw_small_num(8, 128, buf, COLOR_WHITE, COLOR_BLACK, true);

    snprintf(buf, sizeof(buf), "%05lu", (unsigned long)accel_get_steps());
    factory_draw_small_num(20, 144, buf, COLOR_CYAN, COLOR_BLACK, true);
}

// Page 2: Sport Watchface (Giant 16x29 Time + Color Sneaker [37] + Live Steps)
static void render_watchface_page2(void) {
    if (s_diag_first_draw) {
        s_diag_first_draw = false;
        display_fill_screen(COLOR_BLACK);
        gfx_fill_rect(0, 0, 80, 18, COLOR_NAVY);
        factory_draw_utf8(12, 1, "АКТИВНОСТЬ", COLOR_YELLOW, COLOR_NAVY);
        // Draw color Sneaker icon (50x36) at center
        factory_draw_rle_icon(FAC_RLE_STEPS_SHOE, 0, 15, 68);
    }

    // Time HH:MM using Giant 16x29 factory digits (clean row clear prevents ghosts)
    gfx_fill_rect(2, 26, 76, 30, COLOR_BLACK);
    factory_draw_big_time(s_hours, s_minutes, 26, COLOR_CYAN, false);

    char buf[24];
    snprintf(buf, sizeof(buf), "%05lu ШАГ", (unsigned long)accel_get_steps());
    gfx_fill_rect(2, 114, 76, 17, COLOR_BLACK);
    factory_draw_utf8(4, 114, buf, COLOR_GREEN, COLOR_BLACK);

    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", s_hours, s_minutes, s_seconds);
    gfx_fill_rect(2, 140, 76, 17, COLOR_BLACK);
    factory_draw_utf8(10, 140, buf, COLOR_LIGHTGREY, COLOR_BLACK);
}

// Page 3: Factory Menu Icons Showcase (Color RLE + Monochrome 1bpp Icons cycling every 2 seconds)
static void render_watchface_page3(void) {
    static uint8_t s_last_pair = 0xFF;
    uint8_t pair_idx = (uint8_t)((s_uptime_seconds / 2U) % 2U);

    if (s_diag_first_draw || pair_idx != s_last_pair) {
        s_diag_first_draw = false;
        s_last_pair = pair_idx;
        display_fill_screen(COLOR_BLACK);
        gfx_fill_rect(0, 0, 80, 18, COLOR_NAVY);
        factory_draw_utf8(4, 1, "ЗНАЧКИ ПЗУ", COLOR_CYAN, COLOR_NAVY);

        static const uint8_t s_mono_ids[2] = {
            FAC_MONO_TROPHY, FAC_MONO_ALARM
        };
        static const uint16_t s_mono_cols[2] = {
            COLOR_YELLOW, COLOR_CYAN
        };

        // Top: Color RLE icon (Sneaker 50x36)
        factory_draw_rle_icon(FAC_RLE_STEPS_SHOE, 0, 15, 24);

        // Bottom: 1bpp Monochrome icon (Trophy / Alarm 41x41)
        factory_draw_mono_icon(s_mono_ids[pair_idx], 20, 80, s_mono_cols[pair_idx], COLOR_BLACK);

        char buf[20];
        snprintf(buf, sizeof(buf), "Пара %d из 2", pair_idx + 1);
        factory_draw_utf8(6, 142, buf, COLOR_LIGHTGREY, COLOR_BLACK);
    }
}

// Page 4: Factory 16px Russian Cyrillic Font Demo (PHY_FONTV1.01.03)
static void render_watchface_page4(void) {
    uint8_t bat_pct = battery_get_percent();
    if (s_diag_first_draw) {
        s_diag_first_draw = false;
        display_fill_screen(COLOR_BLACK);
        gfx_fill_rect(0, 0, 80, 18, COLOR_NAVY);
        factory_draw_utf8(3, 1, "ШРИФТ ПЗУ", COLOR_YELLOW, COLOR_NAVY);
        gfx_draw_fast_h_line(2, 19, 76, COLOR_DARKGREY);
        gfx_draw_fast_h_line(2, 108, 76, COLOR_DARKGREY);
        factory_draw_utf8(2, 111, "АБВГДЕЁЖЗ", COLOR_ORANGE, COLOR_BLACK);
        factory_draw_utf8(2, 127, "абвгдеёжз", COLOR_LIGHTGREY, COLOR_BLACK);
        factory_draw_utf8(2, 143, "Нажми: след", COLOR_CYAN, COLOR_BLACK);
    }

    char buf[32];
    snprintf(buf, sizeof(buf), "Час: %02d:%02d", s_hours, s_minutes);
    gfx_fill_rect(2, 22, 76, 17, COLOR_BLACK);
    factory_draw_utf8(2, 22, buf, COLOR_WHITE, COLOR_BLACK);

    snprintf(buf, sizeof(buf), "Шаги: %-5lu", (unsigned long)accel_get_steps());
    gfx_fill_rect(2, 43, 76, 17, COLOR_BLACK);
    factory_draw_utf8(2, 43, buf, COLOR_GREEN, COLOR_BLACK);

    snprintf(buf, sizeof(buf), "Имп: %-6lu", (unsigned long)accel_get_pulses());
    gfx_fill_rect(2, 64, 76, 17, COLOR_BLACK);
    factory_draw_utf8(2, 64, buf, COLOR_CYAN, COLOR_BLACK);

    snprintf(buf, sizeof(buf), "Бат: %3d%%", bat_pct);
    gfx_fill_rect(2, 85, 76, 17, COLOR_BLACK);
    factory_draw_utf8(2, 85, buf, (bat_pct < 20) ? COLOR_RED : COLOR_YELLOW, COLOR_BLACK);
}

// Page 5: Factory ROM Sprites Gallery (0x11072000)
static void render_watchface_page5(void) {
    if (s_diag_first_draw) {
        s_diag_first_draw = false;
        display_fill_screen(COLOR_BLACK);
        gfx_fill_rect(0, 0, 80, 18, COLOR_NAVY);
        factory_draw_utf8(6, 1, "СПРАЙТЫ", COLOR_CYAN, COLOR_NAVY);

        for (uint8_t i = 0; i < 5; i++) {
            factory_draw_sprite_1bpp(FACTORY_SPRITE_BAT_BASE + (uint32_t)i * 26U,
                                     2 + (i % 3) * 26, 22 + (i / 3) * 12,
                                     COLOR_GREEN, COLOR_BLACK, false);
        }
        factory_draw_sprite_1bpp(FACTORY_SPRITE_BLE_ADDR, 54, 34, COLOR_CYAN, COLOR_BLACK, false);
        factory_draw_sprite_1bpp(FACTORY_SPRITE_DIV_ADDR, 28, 48, COLOR_DARKGREY, COLOR_BLACK, false);

        factory_draw_sprite_1bpp(FACTORY_SPRITE_WKDAY_BASE + 0 * 50U, 4, 54, COLOR_YELLOW, COLOR_BLACK, false);
        factory_draw_sprite_1bpp(FACTORY_SPRITE_WKDAY_BASE + 1 * 50U, 44, 54, COLOR_ORANGE, COLOR_BLACK, false);
        factory_draw_sprite_1bpp(FACTORY_SPRITE_WKDAY_BASE + 4 * 50U, 4, 68, COLOR_CYAN, COLOR_BLACK, false);
        factory_draw_sprite_1bpp(FACTORY_SPRITE_WKDAY_BASE + 6 * 50U, 44, 68, COLOR_RED, COLOR_BLACK, false);

        factory_draw_small_num(0, 84, "0123456789", COLOR_WHITE, COLOR_BLACK, false);
    }

    uint8_t d0 = (s_seconds / 10) % 10;
    uint8_t d1 = s_seconds % 10;
    gfx_fill_rect(0, 100, 80, 32, COLOR_BLACK);
    factory_draw_sprite_1bpp(FACTORY_SPRITE_BGDIG_BASE + 0 * 68U, 4, 100, COLOR_GREEN, COLOR_BLACK, false);
    factory_draw_sprite_1bpp(FACTORY_SPRITE_BGDIG_BASE + 7 * 68U, 20, 100, COLOR_YELLOW, COLOR_BLACK, false);
    factory_draw_sprite_1bpp(FACTORY_SPRITE_BGDIG_BASE + 10 * 68U, 36, 100, COLOR_WHITE, COLOR_BLACK, false);
    factory_draw_sprite_1bpp(FACTORY_SPRITE_BGDIG_BASE + (uint32_t)d0 * 68U, 48, 100, COLOR_CYAN, COLOR_BLACK, false);
    factory_draw_sprite_1bpp(FACTORY_SPRITE_BGDIG_BASE + (uint32_t)d1 * 68U, 64, 100, COLOR_CYAN, COLOR_BLACK, false);

    factory_draw_utf8(4, 140, "Стр 6 из 6", COLOR_LIGHTGREY, COLOR_BLACK);
}

static void render_watchface(void) {
    if (s_diag_first_draw) {
        display_fill_screen(COLOR_BLACK);
    }
    switch (s_wf_page) {
        case 0:
        default:
            render_watchface_page0();
            break;
        case 1:
            render_watchface_page1();
            break;
        case 2:
            render_watchface_page2();
            break;
        case 3:
            render_watchface_page3();
            break;
        case 4:
            render_watchface_page4();
            break;
        case 5:
            render_watchface_page5();
            break;
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
    gfx_draw_string(8, 38, p_buf, COLOR_CYAN, COLOR_BLACK, 2);

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
    s_wf_page = 0;
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
    accel_init();
    
    s_needs_redraw = false;
    render_watchface();
}

void app_os_update(void) {
    if (ota_inapp_is_active()) {
        vibrator_stop();
        uart_cli_update();
        return;
    }

    s_ms_counter += 50;
    vibrator_update(50);
    accel_update();

    // 5 Hz animation tick for Page 3 (beating heart + ECG)
    if (s_current_mode == MODE_WATCHFACE && s_wf_page == 3 && (s_ms_counter % 200) == 0) {
        s_anim_frame++;
        s_needs_redraw = true;
    }

    // Live motion display refresh (throttled to 4 Hz / 250ms)
    static uint32_t s_last_motion_redraw = 0;
    static uint32_t s_last_drawn_pulses = 0;
    static uint32_t s_last_drawn_steps = 0;
    uint32_t cur_p = accel_get_pulses();
    uint32_t cur_s = accel_get_steps();
    if ((cur_p != s_last_drawn_pulses || cur_s != s_last_drawn_steps) &&
        (s_ms_counter - s_last_motion_redraw >= 250)) {
        s_last_drawn_pulses = cur_p;
        s_last_drawn_steps = cur_s;
        s_last_motion_redraw = s_ms_counter;
        if (s_current_mode == MODE_WATCHFACE) {
            s_needs_redraw = true;
        }
    }

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
        if (ble_get_state() == 0 && ble_get_start_calls() == 0) {
            ble_try_start_device();
        }

        if (s_current_mode == MODE_WATCHFACE) {
            s_needs_redraw = true;
        }
    }

    button_event_t evt = button_update(s_ms_counter);

    if (evt == BTN_EVENT_CLICK) {
        s_click_count++;
        s_vib_count++;

        if (s_current_mode == MODE_WATCHFACE) {
            s_wf_page = (s_wf_page + 1) % WF_PAGE_COUNT;
            s_diag_first_draw = true;
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
            vibrator_toggle_active_level();
            vibrator_test_fire();
        } else {
            s_current_mode = MODE_WATCHFACE;
            s_wf_page = 0;
            s_diag_first_draw = true;
            ble_try_start_device();
        }
        s_needs_redraw = true;
    }

    if (evt == BTN_EVENT_LONG_PRESS) {
        s_click_count += 5;
        s_vib_count += 3;

        if (s_current_mode == MODE_OTA_UPDATE) {
            ble_start_ota_mode();
            return;
        } else if (s_current_mode == MODE_VIBRATOR_TEST) {
            vibrator_select_current_pin();
            s_current_mode = MODE_WATCHFACE;
            s_diag_first_draw = true;
        } else if (s_current_mode == MODE_WATCHFACE) {
            s_current_mode = MODE_OTA_UPDATE;
        }
        s_needs_redraw = true;
    }

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
