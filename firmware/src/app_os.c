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

typedef struct {
    int8_t hours;
    int8_t minutes;
    int8_t seconds;
    int8_t ble_state;
    int8_t bat_pct;
    int8_t bat_charging;
    int16_t bat_mv;
    int32_t steps;
    int32_t pulses;
    int8_t p18;
    uint32_t uptime_s;
    int8_t wk_idx;
} page_cache_t;

static page_cache_t s_cache;

static void cache_invalidate(void) {
    memset(&s_cache, 0xFF, sizeof(s_cache));
}

// Page 0: Main Watchface with Differential Updates
static void render_watchface_page0(bool first) {
    uint8_t bat_pct = battery_get_percent();
    uint16_t bat_mv = battery_read_mv();
    bool ble_conn = ble_is_connected();
    bool charging = battery_is_charging();
    uint32_t steps = accel_get_steps();
    uint32_t pulses = accel_get_pulses();
    uint8_t p18 = hal_gpio_read(ACCEL_INT_PIN);

    if (first) {
        // Header (Navy Blue Bar)
        gfx_fill_rect(0, 0, 80, 15, COLOR_NAVY);
        gfx_draw_string(23, 4, "LP715", COLOR_WHITE, COLOR_NAVY, 1);
        
        gfx_draw_fast_h_line(2, 17, 76, COLOR_DARKGREY);
        gfx_draw_fast_h_line(2, 38, 76, COLOR_DARKGREY);
        gfx_draw_fast_h_line(2, 82, 76, COLOR_DARKGREY);
        gfx_draw_fast_h_line(2, 104, 76, COLOR_DARKGREY);
        gfx_draw_fast_h_line(2, 134, 76, COLOR_DARKGREY);
        
        gfx_draw_string(4, 41, "STEPS:", COLOR_YELLOW, COLOR_BLACK, 1);
        gfx_draw_string(4, 120, FW_VERSION_STR, COLOR_LIGHTGREY, COLOR_BLACK, 1);
        gfx_draw_string(4, 148, "HOLD: OTA", COLOR_CYAN, COLOR_BLACK, 1);
    }

    char buf[24];

    // 0. Header Bar Icons (only redraw when changed)
    if (first || s_cache.ble_state != (int8_t)ble_conn) {
        s_cache.ble_state = (int8_t)ble_conn;
        gfx_draw_ble_icon(3, 3, ble_conn);
        gfx_fill_rect(4, 137, 72, 8, COLOR_BLACK);
        if (ble_conn) {
            gfx_draw_string(4, 137, "BLE:ONLINE", COLOR_GREEN, COLOR_BLACK, 1);
        } else {
            gfx_draw_string(4, 137, "BLE:ADV...", COLOR_YELLOW, COLOR_BLACK, 1);
        }
    }
    if (first || s_cache.bat_pct != (int8_t)bat_pct || s_cache.bat_charging != (int8_t)charging) {
        s_cache.bat_pct = (int8_t)bat_pct;
        s_cache.bat_charging = (int8_t)charging;
        gfx_draw_battery_icon(58, 3, bat_pct, charging);
    }

    // 1. Clock HH:MM (only redraw when hour or minute changes!)
    if (first || s_cache.hours != (int8_t)s_hours || s_cache.minutes != (int8_t)s_minutes) {
        s_cache.hours = (int8_t)s_hours;
        s_cache.minutes = (int8_t)s_minutes;
        gfx_fill_rect(6, 20, 58, 16, COLOR_BLACK);
        snprintf(buf, sizeof(buf), "%02d:%02d", s_hours, s_minutes);
        gfx_draw_string(6, 20, buf, COLOR_CYAN, COLOR_BLACK, 2);
    }

    // Seconds (only a tiny 12x8 box flips every second!)
    if (first || s_cache.seconds != (int8_t)s_seconds) {
        s_cache.seconds = (int8_t)s_seconds;
        gfx_fill_rect(66, 27, 12, 8, COLOR_BLACK);
        snprintf(buf, sizeof(buf), "%02d", s_seconds);
        gfx_draw_string(66, 27, buf, COLOR_YELLOW, COLOR_BLACK, 1);
    }

    // 2. Motion / Steps Row
    if (first || s_cache.p18 != (int8_t)p18) {
        s_cache.p18 = (int8_t)p18;
        gfx_fill_rect(46, 41, 30, 8, COLOR_BLACK);
        snprintf(buf, sizeof(buf), "P18:%d", p18);
        gfx_draw_string(46, 41, buf, COLOR_LIGHTGREY, COLOR_BLACK, 1);
    }

    // Large Step Counter
    if (first || s_cache.steps != (int32_t)steps) {
        s_cache.steps = (int32_t)steps;
        gfx_fill_rect(10, 52, 60, 16, COLOR_BLACK);
        snprintf(buf, sizeof(buf), "%5lu", (unsigned long)steps);
        gfx_draw_string(10, 52, buf, COLOR_GREEN, COLOR_BLACK, 2);
    }

    // Raw pulses
    if (first || s_cache.pulses != (int32_t)pulses) {
        s_cache.pulses = (int32_t)pulses;
        gfx_fill_rect(4, 71, 72, 8, COLOR_BLACK);
        snprintf(buf, sizeof(buf), "PULSE:%-5lu", (unsigned long)pulses);
        gfx_draw_string(4, 71, buf, COLOR_CYAN, COLOR_BLACK, 1);
    }

    // 3. Battery status & Mini-gauge (only redraws when mV or % changes!)
    if (first || s_cache.bat_mv != (int16_t)bat_mv || s_cache.bat_pct != (int8_t)bat_pct) {
        s_cache.bat_mv = (int16_t)bat_mv;
        gfx_fill_rect(4, 85, 72, 17, COLOR_BLACK);
        snprintf(buf, sizeof(buf), "%3d%% %4dmV", bat_pct, bat_mv);
        gfx_draw_string(4, 85, buf, (bat_pct < 20) ? COLOR_RED : COLOR_GREEN, COLOR_BLACK, 1);
        gfx_draw_progress_bar(4, 95, 72, 5, bat_pct, (bat_pct < 20) ? COLOR_RED : COLOR_GREEN, COLOR_DARKGREY);
    }

    // 4. Dynamic Uptime: UP:HH:MM:SS
    if (first || s_cache.uptime_s != s_uptime_seconds) {
        s_cache.uptime_s = s_uptime_seconds;
        uint32_t sec = s_uptime_seconds;
        uint32_t h = sec / 3600;
        uint32_t m = (sec % 3600) / 60;
        uint32_t s = sec % 60;
        gfx_fill_rect(4, 108, 72, 8, COLOR_BLACK);
        snprintf(buf, sizeof(buf), "UP:%02lu:%02lu:%02lu", (unsigned long)h, (unsigned long)m, (unsigned long)s);
        gfx_draw_string(4, 108, buf, COLOR_WHITE, COLOR_BLACK, 1);
    }
}

// Page 1: Factory Watchface #1 (Wallpaper + big digits + ROM icons)
static void render_watchface_page1(bool first) {
    uint8_t bat_pct = battery_get_percent();
    bool ble_conn = ble_is_connected();
    uint8_t bat_idx = bat_pct / 25;
    if (bat_idx > 4) bat_idx = 4;
    uint8_t wk_idx = (uint8_t)((s_uptime_seconds / 3600U) % 7U);
    uint32_t steps = accel_get_steps();

    if (first) {
        factory_draw_bmp_wallpaper();
    }

    if (first || s_cache.ble_state != (int8_t)ble_conn) {
        s_cache.ble_state = (int8_t)ble_conn;
        factory_draw_sprite_1bpp(FACTORY_SPRITE_BLE_ADDR, 4, 4,
                                 ble_conn ? COLOR_CYAN : COLOR_LIGHTGREY, COLOR_BLACK, true);
    }

    if (first || s_cache.bat_pct != (int8_t)bat_idx) {
        s_cache.bat_pct = (int8_t)bat_idx;
        factory_draw_sprite_1bpp(FACTORY_SPRITE_BAT_BASE + (uint32_t)bat_idx * 26U, 60, 4,
                                 COLOR_GREEN, COLOR_BLACK, true);
    }

    if (first || s_cache.wk_idx != (int8_t)wk_idx) {
        s_cache.wk_idx = (int8_t)wk_idx;
        factory_draw_sprite_1bpp(FACTORY_SPRITE_WKDAY_BASE + (uint32_t)wk_idx * 50U, 24, 18,
                                 COLOR_YELLOW, COLOR_BLACK, true);
    }

    if (first || s_cache.hours != (int8_t)s_hours || s_cache.minutes != (int8_t)s_minutes) {
        s_cache.hours = (int8_t)s_hours;
        s_cache.minutes = (int8_t)s_minutes;
        factory_draw_big_time(s_hours, s_minutes, 34, COLOR_WHITE, true);
    }

    if (first || s_cache.seconds != (int8_t)s_seconds) {
        s_cache.seconds = (int8_t)s_seconds;
        char buf[16];
        snprintf(buf, sizeof(buf), "25/10-%02d", s_seconds);
        factory_draw_small_num(8, 128, buf, COLOR_WHITE, COLOR_BLACK, true);
    }

    if (first || s_cache.steps != (int32_t)steps) {
        s_cache.steps = (int32_t)steps;
        char buf[16];
        snprintf(buf, sizeof(buf), "%05lu", (unsigned long)steps);
        factory_draw_small_num(20, 144, buf, COLOR_CYAN, COLOR_BLACK, true);
    }
}

// Page 2: Sport Watchface (Giant 16x29 Time + Color Sneaker [37] + Live Steps)
static void render_watchface_page2(bool first) {
    uint32_t steps = accel_get_steps();

    if (first) {
        gfx_fill_rect(0, 0, 80, 18, COLOR_NAVY);
        factory_draw_utf8(12, 1, "АКТИВНОСТЬ", COLOR_YELLOW, COLOR_NAVY);
        factory_draw_rle_icon(FAC_RLE_STEPS_SHOE, 0, 15, 68);
    }

    if (first || s_cache.hours != (int8_t)s_hours || s_cache.minutes != (int8_t)s_minutes) {
        s_cache.hours = (int8_t)s_hours;
        s_cache.minutes = (int8_t)s_minutes;
        gfx_fill_rect(2, 26, 76, 30, COLOR_BLACK);
        factory_draw_big_time(s_hours, s_minutes, 26, COLOR_CYAN, false);
    }

    if (first || s_cache.steps != (int32_t)steps) {
        s_cache.steps = (int32_t)steps;
        char buf[24];
        snprintf(buf, sizeof(buf), "%05lu ШАГ", (unsigned long)steps);
        gfx_fill_rect(2, 114, 76, 17, COLOR_BLACK);
        factory_draw_utf8(4, 114, buf, COLOR_GREEN, COLOR_BLACK);
    }

    if (first || s_cache.seconds != (int8_t)s_seconds) {
        s_cache.seconds = (int8_t)s_seconds;
        char buf[24];
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", s_hours, s_minutes, s_seconds);
        gfx_fill_rect(2, 140, 76, 17, COLOR_BLACK);
        factory_draw_utf8(10, 140, buf, COLOR_LIGHTGREY, COLOR_BLACK);
    }
}

// Page 3: Factory Menu Icons Showcase (Color RLE + Monochrome 1bpp Icons cycling every 2 seconds)
static void render_watchface_page3(void) {
    static uint8_t s_last_pair = 0xFF;
    uint8_t pair_idx = (uint8_t)((s_uptime_seconds / 2U) % 2U);

    if (pair_idx != s_last_pair) {
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

        factory_draw_rle_icon(FAC_RLE_STEPS_SHOE, 0, 15, 24);
        factory_draw_mono_icon(s_mono_ids[pair_idx], 20, 80, s_mono_cols[pair_idx], COLOR_BLACK);

        char buf[20];
        snprintf(buf, sizeof(buf), "Пара %d из 2", pair_idx + 1);
        factory_draw_utf8(6, 142, buf, COLOR_LIGHTGREY, COLOR_BLACK);
    }
}

// Page 4: Factory 16px Russian Cyrillic Font Demo (PHY_FONTV1.01.03)
static void render_watchface_page4(bool first) {
    uint8_t bat_pct = battery_get_percent();
    uint32_t steps = accel_get_steps();
    uint32_t pulses = accel_get_pulses();

    if (first) {
        gfx_fill_rect(0, 0, 80, 18, COLOR_NAVY);
        factory_draw_utf8(3, 1, "ШРИФТ ПЗУ", COLOR_YELLOW, COLOR_NAVY);
        gfx_draw_fast_h_line(2, 19, 76, COLOR_DARKGREY);
        gfx_draw_fast_h_line(2, 108, 76, COLOR_DARKGREY);
        factory_draw_utf8(2, 111, "АБВГДЕЁЖЗ", COLOR_ORANGE, COLOR_BLACK);
        factory_draw_utf8(2, 127, "абвгдеёжз", COLOR_LIGHTGREY, COLOR_BLACK);
        factory_draw_utf8(2, 143, "Нажми: след", COLOR_CYAN, COLOR_BLACK);
    }

    char buf[32];
    if (first || s_cache.hours != (int8_t)s_hours || s_cache.minutes != (int8_t)s_minutes) {
        s_cache.hours = (int8_t)s_hours;
        s_cache.minutes = (int8_t)s_minutes;
        snprintf(buf, sizeof(buf), "Час: %02d:%02d", s_hours, s_minutes);
        gfx_fill_rect(2, 22, 76, 17, COLOR_BLACK);
        factory_draw_utf8(2, 22, buf, COLOR_WHITE, COLOR_BLACK);
    }

    if (first || s_cache.steps != (int32_t)steps) {
        s_cache.steps = (int32_t)steps;
        snprintf(buf, sizeof(buf), "Шаги: %-5lu", (unsigned long)steps);
        gfx_fill_rect(2, 43, 76, 17, COLOR_BLACK);
        factory_draw_utf8(2, 43, buf, COLOR_GREEN, COLOR_BLACK);
    }

    if (first || s_cache.pulses != (int32_t)pulses) {
        s_cache.pulses = (int32_t)pulses;
        snprintf(buf, sizeof(buf), "Имп: %-6lu", (unsigned long)pulses);
        gfx_fill_rect(2, 64, 76, 17, COLOR_BLACK);
        factory_draw_utf8(2, 64, buf, COLOR_CYAN, COLOR_BLACK);
    }

    if (first || s_cache.bat_pct != (int8_t)bat_pct) {
        s_cache.bat_pct = (int8_t)bat_pct;
        snprintf(buf, sizeof(buf), "Бат: %3d%%", bat_pct);
        gfx_fill_rect(2, 85, 76, 17, COLOR_BLACK);
        factory_draw_utf8(2, 85, buf, (bat_pct < 20) ? COLOR_RED : COLOR_YELLOW, COLOR_BLACK);
    }
}

// Page 5: Factory ROM Sprites Gallery (0x11072000)
static void render_watchface_page5(bool first) {
    if (first) {
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

        factory_draw_sprite_1bpp(FACTORY_SPRITE_BGDIG_BASE + 0 * 68U, 4, 100, COLOR_GREEN, COLOR_BLACK, false);
        factory_draw_sprite_1bpp(FACTORY_SPRITE_BGDIG_BASE + 7 * 68U, 20, 100, COLOR_YELLOW, COLOR_BLACK, false);
        factory_draw_sprite_1bpp(FACTORY_SPRITE_BGDIG_BASE + 10 * 68U, 36, 100, COLOR_WHITE, COLOR_BLACK, false);

        factory_draw_utf8(4, 140, "Стр 6 из 6", COLOR_LIGHTGREY, COLOR_BLACK);
    }

    uint8_t d0 = (s_seconds / 10) % 10;
    uint8_t d1 = s_seconds % 10;

    // Only redraw tens if changed!
    if (first || s_cache.seconds / 10 != (int8_t)d0) {
        factory_draw_sprite_1bpp(FACTORY_SPRITE_BGDIG_BASE + (uint32_t)d0 * 68U, 48, 100, COLOR_CYAN, COLOR_BLACK, false);
    }
    // Only redraw ones if changed!
    if (first || s_cache.seconds % 10 != (int8_t)d1) {
        factory_draw_sprite_1bpp(FACTORY_SPRITE_BGDIG_BASE + (uint32_t)d1 * 68U, 64, 100, COLOR_CYAN, COLOR_BLACK, false);
    }
    s_cache.seconds = (int8_t)s_seconds;
}

static void render_watchface(void) {
    bool first = s_diag_first_draw;
    if (first) {
        s_diag_first_draw = false;
        cache_invalidate();
        if (s_wf_page != 1) {
            display_fill_screen(COLOR_BLACK);
        }
    }
    switch (s_wf_page) {
        case 0:
        default:
            render_watchface_page0(first);
            break;
        case 1:
            render_watchface_page1(first);
            break;
        case 2:
            render_watchface_page2(first);
            break;
        case 3:
            render_watchface_page3();
            break;
        case 4:
            render_watchface_page4(first);
            break;
        case 5:
            render_watchface_page5(first);
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

static char s_notification_text[160] = {0};
static uint16_t s_notification_color = COLOR_WHITE;
static uint8_t s_notification_scale = 1;

void app_os_show_message(const char *text, uint16_t color, uint8_t scale) {
    if (!text) return;
    strncpy(s_notification_text, text, sizeof(s_notification_text) - 1);
    s_notification_text[sizeof(s_notification_text) - 1] = '\0';
    s_notification_color = color ? color : COLOR_WHITE;
    s_notification_scale = (scale == 2) ? 2 : 1;
    s_current_mode = MODE_NOTIFICATION;
    s_diag_first_draw = true;
    s_needs_redraw = true;
    vibrator_pulse(120);
}

void app_os_dismiss_message(void) {
    if (s_current_mode == MODE_NOTIFICATION) {
        s_current_mode = MODE_WATCHFACE;
        s_diag_first_draw = true;
        s_needs_redraw = true;
    }
}

static void render_notification(void) {
    display_fill_screen(COLOR_BLACK);

    // Header banner
    gfx_fill_rect(0, 0, 80, 15, COLOR_NAVY);
    gfx_draw_string_utf8(6, 4, "СООБЩЕНИЕ", COLOR_YELLOW, COLOR_NAVY, 1);
    gfx_draw_fast_h_line(2, 17, 76, COLOR_DARKGREY);

    // Body with automatic word wrapping
    gfx_draw_string_utf8(4, 22, s_notification_text, s_notification_color, COLOR_BLACK, s_notification_scale);

    // Footer
    gfx_draw_fast_h_line(2, 144, 76, COLOR_DARKGREY);
    gfx_draw_string_utf8(8, 148, "КЛИК: ВЫХОД", COLOR_CYAN, COLOR_BLACK, 1);
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
        if (s_current_mode == MODE_NOTIFICATION) {
            app_os_dismiss_message();
            vibrator_pulse(50);
            return;
        }

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
            case MODE_NOTIFICATION:
                render_notification();
                break;
            default:
                break;
        }
    }
}
