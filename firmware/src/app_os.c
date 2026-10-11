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
#include <stdio.h>
#include <string.h>

#define COLOR_NAVY 0x000F

static void fmt_2digits(char *buf, uint8_t val) {
    buf[0] = '0' + ((val / 10) % 10);
    buf[1] = '0' + (val % 10);
    buf[2] = '\0';
}

static os_mode_t s_current_mode = MODE_WATCHFACE;
static uint32_t s_uptime_seconds = 0;
static uint32_t s_last_sec_tick = 0;
static uint32_t s_ms_counter = 0;

// Watch Time (Simulated RTC starting at 12:34:00)
static uint8_t s_hours = 12;
static uint8_t s_minutes = 34;
static uint8_t s_seconds = 0;
static uint8_t s_battery = 88;

// Cheat Sheet Data
static const char *s_cheat_tickets[] = {
    // Ticket 1: Physics formulas
    "Билет 1: Физика\n"
    "E = mc^2\n"
    "F = m*a\n"
    "p = m*v\n"
    "A = F*s*cos(a)\n"
    "E_k = (m*v^2)/2\n"
    "E_p = m*g*h\n"
    "N = A/t\n"
    "P = F/S\n"
    "p = p0 + p*g*h\n"
    "Fa = p_ж*g*V\n"
    "T = 2*pi*sqrt(l/g)\n"
    "T = 2*pi*sqrt(m/k)",

    // Ticket 2: Math formulas
    "Билет 2: Математика\n"
    "D = b^2 - 4*a*c\n"
    "x1,2 = (-b+-VD)/2a\n"
    "sin^2+cos^2 = 1\n"
    "sin(2a)=2sin*cos\n"
    "cos(2a)=cos^2-sin^2\n"
    "(a+b)^2=a^2+2ab+b^2\n"
    "a^2-b^2=(a-b)(a+b)\n"
    "log_a(b*c)=lga+lgc\n"
    "(u*v)'=u'v + uv'\n"
    "(u/v)'=(u'v-uv')/v2\n"
    "int x^n = x^(n+1)/n",

    // Ticket 3: English / History
    "Билет 3: Даты\n"
    "862 - призв. варяг\n"
    "988 - Крещение Руси\n"
    "1242 - Ледовое поб.\n"
    "1380 - Куликовская\n"
    "1480 - Стояние Угре\n"
    "1612 - Ополчение\n"
    "1703 - СПб основан\n"
    "1709 - Полтава\n"
    "1812 - Бородино\n"
    "1861 - Отмена КП\n"
    "1917 - Революция\n"
    "1945 - Победа ВОВ",

    // Ticket 4: Chemistry / Laws
    "Билет 4: Химия/Законы\n"
    "PV = nRT (Менд-Клап)\n"
    "n = m / M\n"
    "Vm = 22.4 л/моль\n"
    "pH = -lg[H+]\n"
    "Q = c*m*dT\n"
    "I = U / R (Закон Ома)\n"
    "P = I * U = I^2 * R\n"
    "B = u0*I / (2*pi*r)\n"
    "E = h * nu\n"
    "c = 3*10^8 м/с\n"
    "Na = 6.02*10^23"
};

#define TOTAL_TICKETS 4
static uint8_t s_current_ticket = 0;

// Stopwatch State
static bool s_sw_running = false;
static uint32_t s_sw_elapsed_ms = 0;
static uint32_t s_sw_start_ms = 0;

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
        gfx_draw_string(4, 142, "CLICK = MENU", COLOR_GREEN, COLOR_BLACK, 1);
        gfx_draw_string(4, 152, "HOLD = OTA", COLOR_CYAN, COLOR_BLACK, 1);
    }

    // Hardware status: Uptime & Battery
    char buf[20];
    snprintf(buf, sizeof(buf), "UP: %-7lus", (unsigned long)s_uptime_seconds);
    gfx_draw_string(4, 18, buf, COLOR_WHITE, COLOR_BLACK, 1);

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
    gfx_draw_string(4, 30, buf, st_col, COLOR_BLACK, 1);

    snprintf(buf, sizeof(buf), "B:%d C:%d R:%02X", bst, cst, bret);
    gfx_draw_string(4, 42, buf, COLOR_LIGHTGREY, COLOR_BLACK, 1);

    snprintf(buf, sizeof(buf), "Calls: %-4u", (unsigned int)bcalls);
    gfx_draw_string(4, 54, buf, COLOR_LIGHTGREY, COLOR_BLACK, 1);

    // Button input counter
    snprintf(buf, sizeof(buf), "BTN: %-4lu evt", (unsigned long)s_click_count);
    gfx_draw_string(4, 110, buf, COLOR_WHITE, COLOR_BLACK, 1);

    // Vibrator pulse counter
    snprintf(buf, sizeof(buf), "VIB: %-4lu buzz", (unsigned long)s_vib_count);
    gfx_draw_string(4, 122, buf, COLOR_WHITE, COLOR_BLACK, 1);
}

static void render_cheatsheet(void) {
    display_fill_screen(COLOR_BLACK);

    // Header bar with ticket index: e.g. "[1/4] ШПОРА"
    char hdr[24];
    hdr[0] = '[';
    hdr[1] = '0' + (s_current_ticket + 1);
    hdr[2] = '/';
    hdr[3] = '0' + TOTAL_TICKETS;
    hdr[4] = ']';
    hdr[5] = ' ';
    hdr[6] = '\0';
    strcat(hdr, "ШПОРА");
    gfx_draw_rect(0, 0, 80, 12, COLOR_DARKGREY);
    gfx_draw_string_utf8(4, 2, hdr, COLOR_YELLOW, COLOR_BLACK, 1);

    // Ticket text in stealth micro-font (scale 1 = 6x8 pixels per char)
    // 80px width fits 13 characters per line!
    // 148px height fits 18 lines!
    gfx_draw_string_utf8(2, 15, s_cheat_tickets[s_current_ticket], COLOR_WHITE, COLOR_BLACK, 1);

    // Footer stealth hint
    gfx_draw_fast_h_line(0, 158, 80, COLOR_DARKGREY);
}

static void render_stopwatch(void) {
    display_fill_screen(COLOR_BLACK);

    gfx_draw_string_utf8(10, 8, "СЕКУНДОМЕР", COLOR_YELLOW, COLOR_BLACK, 1);
    gfx_draw_fast_h_line(2, 22, 76, COLOR_DARKGREY);

    uint32_t total_ms = s_sw_elapsed_ms;
    if (s_sw_running) total_ms += (s_ms_counter - s_sw_start_ms);

    uint32_t secs = total_ms / 1000;
    uint32_t mins = secs / 60;
    secs %= 60;
    uint32_t tenths = (total_ms % 1000) / 100;

    char sw_buf[16];
    fmt_2digits(sw_buf, mins);
    sw_buf[2] = ':';
    fmt_2digits(sw_buf + 3, secs);
    gfx_draw_string(10, 48, sw_buf, COLOR_GREEN, COLOR_BLACK, 2);

    sw_buf[0] = '.';
    sw_buf[1] = '0' + (tenths % 10);
    sw_buf[2] = '\0';
    gfx_draw_string(30, 72, sw_buf, COLOR_WHITE, COLOR_BLACK, 2);

    if (s_sw_running) {
        gfx_draw_string_utf8(20, 110, "[ ИДЁТ ]", COLOR_CYAN, COLOR_BLACK, 1);
        gfx_draw_string_utf8(6, 130, "Клик: Пауза", COLOR_LIGHTGREY, COLOR_BLACK, 1);
    } else {
        gfx_draw_string_utf8(20, 110, "[ СТОП ]", COLOR_RED, COLOR_BLACK, 1);
        gfx_draw_string_utf8(6, 130, "Клик: Старт", COLOR_LIGHTGREY, COLOR_BLACK, 1);
        gfx_draw_string_utf8(6, 142, "2клик: Сброс", COLOR_LIGHTGREY, COLOR_BLACK, 1);
    }
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
    gfx_draw_string(4, 122, "Click: Next", COLOR_LIGHTGREY, COLOR_BLACK, 1);
    gfx_draw_string(4, 136, "HOLD: FLASH", COLOR_CYAN, COLOR_BLACK, 1);
}

static void render_flashlight(void) {
    display_fill_screen(COLOR_WHITE);
}

void app_os_panic(void) {
    // Instant panic action: flip back to harmless clock face and buzz softly
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
    uart_cli_init();
    
    // Initial welcome buzz
    vibrator_pulse(60);
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

        if (s_current_mode == MODE_WATCHFACE || s_current_mode == MODE_STOPWATCH) {
            s_needs_redraw = true;
        }
    }

    // Process button events
    button_event_t evt = button_update(s_ms_counter);

    if (evt == BTN_EVENT_CLICK) {
        s_click_count++;
        s_vib_count++;
        vibrator_pulse(30);

        if (s_current_mode == MODE_WATCHFACE) {
            s_current_mode = MODE_CHEATSHEET;
        } else if (s_current_mode == MODE_CHEATSHEET) {
            s_current_ticket++;
            if (s_current_ticket >= TOTAL_TICKETS) {
                s_current_ticket = 0;
                s_current_mode = MODE_STOPWATCH;
            }
        } else if (s_current_mode == MODE_STOPWATCH) {
            if (s_sw_running) {
                s_sw_elapsed_ms += (s_ms_counter - s_sw_start_ms);
                s_sw_running = false;
            } else {
                s_sw_start_ms = s_ms_counter;
                s_sw_running = true;
            }
        } else if (s_current_mode == MODE_OTA_UPDATE) {
            s_current_mode = MODE_FLASHLIGHT;
        } else if (s_current_mode == MODE_FLASHLIGHT) {
            s_current_mode = MODE_WATCHFACE;
            s_diag_first_draw = true;
        }
        s_needs_redraw = true;
    }

    if (evt == BTN_EVENT_DOUBLE_CLICK) {
        s_click_count += 2;
        s_vib_count += 2;
        vibrator_double_pulse();

        if (s_current_mode == MODE_STOPWATCH) {
            s_sw_running = false;
            s_sw_elapsed_ms = 0;
        } else {
            // Panic or quick home: back to main watch screen
            s_current_mode = MODE_WATCHFACE;
            s_diag_first_draw = true;
            ble_try_start_device();
        }
        s_needs_redraw = true;
    }

    if (evt == BTN_EVENT_LONG_PRESS) {
        s_click_count += 5;
        s_vib_count += 3;
        vibrator_pulse(100);

        if (s_current_mode == MODE_OTA_UPDATE) {
            // REBOOT INTO OTA BOOTLOADER
            ble_start_ota_mode();
            return;
        } else if (s_current_mode == MODE_WATCHFACE) {
            // Fast shortcut to OTA Mode
            s_current_mode = MODE_OTA_UPDATE;
        } else if (s_current_mode == MODE_STOPWATCH) {
            s_current_mode = MODE_OTA_UPDATE;
        } else {
            // Panic from cheatsheet or flashlight
            s_current_mode = MODE_WATCHFACE;
            s_diag_first_draw = true;
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
            case MODE_CHEATSHEET:
                render_cheatsheet();
                break;
            case MODE_STOPWATCH:
                render_stopwatch();
                break;
            case MODE_OTA_UPDATE:
                render_ota_update();
                break;
            case MODE_FLASHLIGHT:
                render_flashlight();
                break;
            default:
                break;
        }
    }
}
