import re
import os

app_os_path = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), 'firmware', 'src', 'app_os.c')

with open(app_os_path, 'r', encoding='utf-8') as f:
    content = f.read()

replacement = '''typedef struct {
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
        gfx_draw_string(4, 120, "v1.5.18-DIFF", COLOR_LIGHTGREY, COLOR_BLACK, 1);
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
}'''

# Replace from static void render_watchface_page0(void) up to the end of render_watchface(void)
pattern = r'// Page 0: Main Watchface.*?static void render_watchface\(void\)\s*\{.*?switch \(s_wf_page\)\s*\{.*?\}\s*\}'
new_content, count = re.subn(pattern, replacement, content, flags=re.DOTALL)
if count != 1:
    print(f"Error: pattern replaced {count} times (expected 1)")
    exit(1)

with open(app_os_path, 'w', encoding='utf-8') as f:
    f.write(new_content)

print("[SUCCESS] Patched app_os.c with differential render cache!")
