#include "app_os.h"
#include "display.h"
#include "gfx.h"
#include "button.h"
#include "vibrator.h"
#include "mcu.h"
#include <stdio.h>
#include <string.h>

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

static void fmt_2digits(char *buf, uint8_t val) {
    buf[0] = '0' + ((val / 10) % 10);
    buf[1] = '0' + (val % 10);
    buf[2] = '\0';
}

static void render_watchface(void) {
    display_fill_screen(COLOR_BLACK);

    // Top Status Bar: Battery, BLE State code, Date
    extern bool ble_is_connected(void);
    extern uint8_t ble_get_state(void);
    gfx_draw_battery_icon(58, 4, s_battery, false);
    gfx_draw_ble_icon(4, 4, ble_is_connected());

    // Display BLE status code: 0=Init, 1=Started, 2=Advertising, 3=Waiting, 4=Connected, 5=Error
    char ble_st_buf[6];
    uint8_t bst = ble_get_state();
    ble_st_buf[0] = 'B';
    ble_st_buf[1] = ':';
    ble_st_buf[2] = '0' + (bst % 10);
    ble_st_buf[3] = '\0';
    gfx_draw_string(14, 5, ble_st_buf, (bst == 2) ? COLOR_GREEN : COLOR_YELLOW, COLOR_BLACK, 1);

    // Separator line
    gfx_draw_fast_h_line(2, 17, 76, COLOR_DARKGREY);

    // Big Digital Clock (HH : MM)
    char time_buf[8];
    fmt_2digits(time_buf, s_hours);
    gfx_draw_string(14, 28, time_buf, COLOR_CYAN, COLOR_BLACK, 3);

    // Animated Colon
    if (s_seconds % 2 == 0) {
        gfx_fill_circle(40, 56, 2, COLOR_WHITE);
        gfx_fill_circle(40, 68, 2, COLOR_WHITE);
    }

    fmt_2digits(time_buf, s_minutes);
    gfx_draw_string(14, 76, time_buf, COLOR_CYAN, COLOR_BLACK, 3);

    // Separator line
    gfx_draw_fast_h_line(2, 110, 76, COLOR_DARKGREY);

    // Step counter simulation & Heart rate icon
    gfx_fill_circle(14, 122, 4, COLOR_RED);
    gfx_draw_string_utf8(24, 118, "72 уд", COLOR_WHITE, COLOR_BLACK, 1);

    gfx_draw_string_utf8(10, 134, "5420 шаг", COLOR_YELLOW, COLOR_BLACK, 1);

    // Bottom Mode Hint
    gfx_draw_string_utf8(12, 150, "МЕНЮ >", COLOR_DARKGREY, COLOR_BLACK, 1);
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

static void render_flashlight(void) {
    display_fill_screen(COLOR_WHITE);
}

void app_os_panic(void) {
    // Instant panic action: flip back to harmless clock face and buzz softly
    s_current_mode = MODE_WATCHFACE;
    s_needs_redraw = true;
    vibrator_pulse(40);
}

void app_os_init(void) {
    display_init(0); // Portrait 80x160
    button_init();
    vibrator_init();
    
    // Initial welcome buzz
    vibrator_pulse(60);
    s_needs_redraw = false;
    render_watchface();
}

void app_os_update(void) {
    s_ms_counter += 20; // Called every 20ms by OSAL timer (non-blocking)

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
        if (s_current_mode == MODE_WATCHFACE || s_current_mode == MODE_STOPWATCH) {
            s_needs_redraw = true;
        }
    }

    // Process button events
    button_event_t evt = button_update(s_ms_counter);

    if (evt == BTN_EVENT_LONG_PRESS) {
        // PANIC BUTTON TRIGGER!
        if (s_current_mode != MODE_WATCHFACE) {
            app_os_panic();
            return;
        } else {
            // Long press from watchface enters stealth cheat sheet!
            s_current_mode = MODE_CHEATSHEET;
            vibrator_pulse(40);
            s_needs_redraw = true;
            return;
        }
    }

    if (evt == BTN_EVENT_CLICK) {
        vibrator_pulse(20); // short click tactile feedback

        switch (s_current_mode) {
            case MODE_WATCHFACE:
                // Single click switches to next tool: Stopwatch -> Flashlight -> Watchface
                s_current_mode = MODE_STOPWATCH;
                s_needs_redraw = true;
                break;

            case MODE_CHEATSHEET:
                // Single click navigates to next ticket page!
                s_current_ticket = (s_current_ticket + 1) % TOTAL_TICKETS;
                s_needs_redraw = true;
                break;

            case MODE_STOPWATCH:
                // Toggle run / stop
                if (s_sw_running) {
                    s_sw_elapsed_ms += (s_ms_counter - s_sw_start_ms);
                    s_sw_running = false;
                } else {
                    s_sw_start_ms = s_ms_counter;
                    s_sw_running = true;
                }
                s_needs_redraw = true;
                break;

            case MODE_FLASHLIGHT:
                s_current_mode = MODE_WATCHFACE;
                s_needs_redraw = true;
                break;

            default:
                break;
        }
    }

    if (evt == BTN_EVENT_DOUBLE_CLICK) {
        vibrator_double_pulse();

        switch (s_current_mode) {
            case MODE_WATCHFACE:
                // Double click jumps straight to flashlight
                s_current_mode = MODE_FLASHLIGHT;
                s_needs_redraw = true;
                break;

            case MODE_CHEATSHEET:
                // Double click goes to previous ticket
                if (s_current_ticket == 0) s_current_ticket = TOTAL_TICKETS - 1;
                else s_current_ticket--;
                s_needs_redraw = true;
                break;

            case MODE_STOPWATCH:
                // Reset stopwatch
                s_sw_running = false;
                s_sw_elapsed_ms = 0;
                s_needs_redraw = true;
                break;

            default:
                break;
        }
    }

    // Redraw screen if requested
    if (s_needs_redraw) {
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
            case MODE_FLASHLIGHT:
                render_flashlight();
                break;
            default:
                break;
        }
    }
}
