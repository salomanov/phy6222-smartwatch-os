#include "gfx.h"
#include "font5x7.h"
#include <stdlib.h>

void gfx_draw_pixel(int16_t x, int16_t y, uint16_t color) {
    display_draw_pixel((uint16_t)x, (uint16_t)y, color);
}

void gfx_draw_fast_h_line(int16_t x, int16_t y, int16_t w, uint16_t color) {
    if (y < 0 || y >= display_get_height() || x >= display_get_width() || (x + w) <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (x + w > display_get_width()) w = display_get_width() - x;
    if (w <= 0) return;

    display_set_addr_window(x, y, x + w - 1, y);
    hal_gpio_write(DC_PIN, 1);
    
    #define H_BUF_SIZE 40
    static uint16_t buf[H_BUF_SIZE];
    for (int i = 0; i < H_BUF_SIZE; i++) buf[i] = (color >> 8) | ((color & 0xFF) << 8);

    while (w > 0) {
        int chunk = (w > H_BUF_SIZE) ? H_BUF_SIZE : w;
        display_fill_window(x, y, x + chunk - 1, y, buf, chunk);
        x += chunk;
        w -= chunk;
    }
}

void gfx_draw_fast_v_line(int16_t x, int16_t y, int16_t h, uint16_t color) {
    if (x < 0 || x >= display_get_width() || y >= display_get_height() || (y + h) <= 0) return;
    if (y < 0) { h += y; y = 0; }
    if (y + h > display_get_height()) h = display_get_height() - y;
    if (h <= 0) return;

    display_set_addr_window(x, y, x, y + h - 1);
    hal_gpio_write(DC_PIN, 1);

    #define V_BUF_SIZE 40
    static uint16_t buf[V_BUF_SIZE];
    for (int i = 0; i < V_BUF_SIZE; i++) buf[i] = (color >> 8) | ((color & 0xFF) << 8);

    while (h > 0) {
        int chunk = (h > V_BUF_SIZE) ? V_BUF_SIZE : h;
        display_fill_window(x, y, x, y + chunk - 1, buf, chunk);
        y += chunk;
        h -= chunk;
    }
}

void gfx_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    if (y0 == y1) {
        if (x1 > x0) gfx_draw_fast_h_line(x0, y0, x1 - x0 + 1, color);
        else gfx_draw_fast_h_line(x1, y0, x0 - x1 + 1, color);
        return;
    }
    if (x0 == x1) {
        if (y1 > y0) gfx_draw_fast_v_line(x0, y0, y1 - y0 + 1, color);
        else gfx_draw_fast_v_line(x0, y1, y0 - y1 + 1, color);
        return;
    }

    int16_t steep = abs(y1 - y0) > abs(x1 - x0);
    if (steep) {
        int16_t t;
        t = x0; x0 = y0; y0 = t;
        t = x1; x1 = y1; y1 = t;
    }
    if (x0 > x1) {
        int16_t t;
        t = x0; x0 = x1; x1 = t;
        t = y0; y0 = y1; y1 = t;
    }

    int16_t dx = x1 - x0;
    int16_t dy = abs(y1 - y0);
    int16_t err = dx / 2;
    int16_t ystep = (y0 < y1) ? 1 : -1;

    for (; x0 <= x1; x0++) {
        if (steep) gfx_draw_pixel(y0, x0, color);
        else gfx_draw_pixel(x0, y0, color);
        err -= dy;
        if (err < 0) {
            y0 += ystep;
            err += dx;
        }
    }
}

void gfx_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    gfx_draw_fast_h_line(x, y, w, color);
    gfx_draw_fast_h_line(x, y + h - 1, w, color);
    gfx_draw_fast_v_line(x, y, h, color);
    gfx_draw_fast_v_line(x + w - 1, y, h, color);
}

void gfx_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (x >= display_get_width() || y >= display_get_height() || w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > display_get_width()) w = display_get_width() - x;
    if (y + h > display_get_height()) h = display_get_height() - y;
    if (w <= 0 || h <= 0) return;

    display_set_addr_window(x, y, x + w - 1, y + h - 1);
    
    #define RECT_BUF_SIZE 64
    static uint16_t buf[RECT_BUF_SIZE];
    for (int i = 0; i < RECT_BUF_SIZE; i++) buf[i] = (color >> 8) | ((color & 0xFF) << 8);

    uint32_t total = (uint32_t)w * h;
    while (total > 0) {
        uint16_t chunk = (total > RECT_BUF_SIZE) ? RECT_BUF_SIZE : total;
        display_fill_window(x, y, x + w - 1, y + h - 1, buf, chunk);
        total -= chunk;
    }
}

void gfx_draw_circle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;

    gfx_draw_pixel(x0, y0 + r, color);
    gfx_draw_pixel(x0, y0 - r, color);
    gfx_draw_pixel(x0 + r, y0, color);
    gfx_draw_pixel(x0 - r, y0, color);

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        gfx_draw_pixel(x0 + x, y0 + y, color);
        gfx_draw_pixel(x0 - x, y0 + y, color);
        gfx_draw_pixel(x0 + x, y0 - y, color);
        gfx_draw_pixel(x0 - x, y0 - y, color);
        gfx_draw_pixel(x0 + y, y0 + x, color);
        gfx_draw_pixel(x0 - y, y0 + x, color);
        gfx_draw_pixel(x0 + y, y0 - x, color);
        gfx_draw_pixel(x0 - y, y0 - x, color);
    }
}

void gfx_fill_circle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
    gfx_draw_fast_v_line(x0, y0 - r, 2 * r + 1, color);
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        gfx_draw_fast_v_line(x0 + x, y0 - y, 2 * y + 1, color);
        gfx_draw_fast_v_line(x0 - x, y0 - y, 2 * y + 1, color);
        gfx_draw_fast_v_line(x0 + y, y0 - x, 2 * x + 1, color);
        gfx_draw_fast_v_line(x0 - y, y0 - x, 2 * x + 1, color);
    }
}

void gfx_draw_round_rect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
    gfx_draw_fast_h_line(x + r, y, w - 2 * r, color);
    gfx_draw_fast_h_line(x + r, y + h - 1, w - 2 * r, color);
    gfx_draw_fast_v_line(x, y + r, h - 2 * r, color);
    gfx_draw_fast_v_line(x + w - 1, y + r, h - 2 * r, color);
}

void gfx_fill_round_rect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
    gfx_fill_rect(x + r, y, w - 2 * r, h, color);
    // Draw edges
    for (int16_t i = 0; i < r; i++) {
        gfx_draw_fast_v_line(x + i, y + r - i, h - 2 * (r - i), color);
        gfx_draw_fast_v_line(x + w - 1 - i, y + r - i, h - 2 * (r - i), color);
    }
}

// 5x7 Font Character Renderer (ASCII 32..126 and Cyrillic CP1251 0xC0..0xFF)
void gfx_draw_char(int16_t x, int16_t y, uint8_t c, uint16_t color, uint16_t bg, uint8_t size) {
    if (x >= display_get_width() || y >= display_get_height() || (x + 6 * size) <= 0 || (y + 8 * size) <= 0) return;

    const uint8_t *glyph_data = 0;
    if (c >= 32 && c <= 126) {
        glyph_data = &font5x7_data[(c - 32) * 5];
    } else if (c >= 0xC0 && c <= 0xFF) {
        glyph_data = &font5x7_cyrillic[(c - 0xC0) * 5];
    } else {
        // Unknown char: space
        glyph_data = &font5x7_data[0];
    }

    for (int8_t i = 0; i < 5; i++) {
        uint8_t line = glyph_data[i];
        for (int8_t j = 0; j < 8; j++) {
            if (line & 0x01) {
                if (size == 1) gfx_draw_pixel(x + i, y + j, color);
                else gfx_fill_rect(x + i * size, y + j * size, size, size, color);
            } else if (bg != color) {
                if (size == 1) gfx_draw_pixel(x + i, y + j, bg);
                else gfx_fill_rect(x + i * size, y + j * size, size, size, bg);
            }
            line >>= 1;
        }
    }
    // 6th column spacing
    if (bg != color) {
        if (size == 1) gfx_draw_fast_v_line(x + 5, y, 8, bg);
        else gfx_fill_rect(x + 5 * size, y, size, 8 * size, bg);
    }
}

static int get_word_len(const char *s) {
    int len = 0;
    while (*s && *s != ' ' && *s != '\n' && *s != '\r') {
        len++;
        s++;
    }
    return len;
}

static int get_utf8_word_chars(const uint8_t *p) {
    int chars = 0;
    while (*p && *p != ' ' && *p != '\n' && *p != '\r') {
        if (*p < 0x80) {
            chars++;
            p++;
        } else if (*p == 0xD0 || *p == 0xD1) {
            chars++;
            p += 2;
        } else {
            p++;
        }
    }
    return chars;
}

void gfx_draw_string(int16_t x, int16_t y, const char *str, uint16_t color, uint16_t bg, uint8_t size) {
    int16_t cur_x = x;
    int16_t cur_y = y;
    int line_height = 8 * size + 2;
    int char_width = 6 * size;
    bool at_word_start = true;

    while (*str) {
        char c = *str;

        if (c == '\n') {
            str++;
            cur_x = x;
            cur_y += line_height;
            at_word_start = true;
            continue;
        } else if (c == '\r') {
            str++;
            continue;
        }

        if (c == ' ') {
            // Drop leading space if we just wrapped to line start
            if (cur_x == x) {
                str++;
                continue;
            }
            int next_word_px = get_word_len(str + 1) * char_width;
            if (cur_x + char_width + next_word_px > display_get_width()) {
                str++; // Wrap before next word
                cur_x = x;
                cur_y += line_height;
                at_word_start = true;
                continue;
            }
            at_word_start = true;
        } else if (at_word_start) {
            at_word_start = false;
            int word_px = get_word_len(str) * char_width;
            if (cur_x > x && cur_x + word_px > display_get_width() && word_px <= (display_get_width() - x)) {
                cur_x = x;
                cur_y += line_height;
            }
        }

        // Hard wrap if character still exceeds screen width
        if (cur_x + char_width > display_get_width()) {
            cur_x = x;
            cur_y += line_height;
        }

        if (cur_y + 8 * size <= display_get_height()) {
            gfx_draw_char(cur_x, cur_y, (uint8_t)c, color, bg, size);
        }
        cur_x += char_width;
        str++;
    }
}

// UTF-8 Decoded String Renderer (Russian and English in standard UTF-8 files)
void gfx_draw_string_utf8(int16_t x, int16_t y, const char *utf8_str, uint16_t color, uint16_t bg, uint8_t size) {
    int16_t cur_x = x;
    int16_t cur_y = y;
    int line_height = 8 * size + 2;
    int char_width = 6 * size;
    const uint8_t *p = (const uint8_t*)utf8_str;
    bool at_word_start = true;

    while (*p) {
        uint8_t c = *p;

        if (c == '\n') {
            p++;
            cur_x = x;
            cur_y += line_height;
            at_word_start = true;
            continue;
        } else if (c == '\r') {
            p++;
            continue;
        }

        if (c == ' ') {
            // Drop leading space if we just wrapped to line start
            if (cur_x == x) {
                p++;
                continue;
            }
            int next_word_px = get_utf8_word_chars(p + 1) * char_width;
            if (cur_x + char_width + next_word_px > display_get_width()) {
                p++; // Wrap before next word
                cur_x = x;
                cur_y += line_height;
                at_word_start = true;
                continue;
            }
            at_word_start = true;
        } else if (at_word_start) {
            at_word_start = false;
            int word_px = get_utf8_word_chars(p) * char_width;
            if (cur_x > x && cur_x + word_px > display_get_width() && word_px <= (display_get_width() - x)) {
                cur_x = x;
                cur_y += line_height;
            }
        }

        p++;
        uint8_t cp1251 = 0;

        if (c < 0x80) {
            // Standard ASCII
            cp1251 = c;
        } else if (c == 0xD0) {
            uint8_t next = *p++;
            if (next == 0x81) cp1251 = 0xC5; // 'Ё' -> 'Е'
            else if (next >= 0x90 && next <= 0xBF) cp1251 = next + 0x30; // А..п
            else cp1251 = '?';
        } else if (c == 0xD1) {
            uint8_t next = *p++;
            if (next == 0x91) cp1251 = 0xE5; // 'ё' -> 'е'
            else if (next >= 0x80 && next <= 0x8F) cp1251 = next + 0x70; // р..я
            else cp1251 = '?';
        } else {
            // Other multibyte: skip
            continue;
        }

        // Hard wrap if character still exceeds screen width
        if (cur_x + char_width > display_get_width()) {
            cur_x = x;
            cur_y += line_height;
        }
        if (cur_y + 8 * size <= display_get_height()) {
            gfx_draw_char(cur_x, cur_y, cp1251, color, bg, size);
        }
        cur_x += char_width;
    }
}

void gfx_draw_battery_icon(int16_t x, int16_t y, uint8_t percent, bool charging) {
    // Battery outline: 16x8 pixels
    gfx_draw_rect(x, y, 16, 9, COLOR_WHITE);
    gfx_draw_fast_v_line(x + 16, y + 2, 5, COLOR_WHITE); // Nipple
    
    uint16_t fill_color = COLOR_GREEN;
    if (percent <= 20) fill_color = COLOR_RED;
    else if (percent <= 50) fill_color = COLOR_YELLOW;

    uint8_t fill_w = (percent * 12) / 100;
    if (fill_w > 12) fill_w = 12;

    gfx_fill_rect(x + 2, y + 2, fill_w, 5, fill_color);
    if (fill_w < 12) {
        gfx_fill_rect(x + 2 + fill_w, y + 2, 12 - fill_w, 5, COLOR_BLACK);
    }

    if (charging) {
        // Draw small lightning bolt symbol in cyan
        gfx_draw_pixel(x + 8, y + 1, COLOR_CYAN);
        gfx_draw_pixel(x + 7, y + 2, COLOR_CYAN);
        gfx_draw_pixel(x + 8, y + 3, COLOR_CYAN);
        gfx_draw_pixel(x + 9, y + 4, COLOR_CYAN);
        gfx_draw_pixel(x + 8, y + 5, COLOR_CYAN);
        gfx_draw_pixel(x + 7, y + 6, COLOR_CYAN);
    }
}

void gfx_draw_ble_icon(int16_t x, int16_t y, bool connected) {
    uint16_t col = connected ? COLOR_CYAN : COLOR_DARKGREY;
    // Bluetooth rune: vertical line and diagonal arrows
    gfx_draw_fast_v_line(x + 3, y, 9, col);
    gfx_draw_line(x + 1, y + 2, x + 5, y + 6, col);
    gfx_draw_line(x + 1, y + 6, x + 5, y + 2, col);
    gfx_draw_line(x + 3, y, x + 5, y + 2, col);
    gfx_draw_line(x + 3, y + 8, x + 5, y + 6, col);
}

void gfx_draw_progress_bar(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t percent, uint16_t color, uint16_t bg) {
    gfx_draw_rect(x, y, w, h, COLOR_WHITE);
    int16_t inner_w = w - 4;
    int16_t inner_h = h - 4;
    int16_t fill_w = (inner_w * percent) / 100;
    if (fill_w > 0) gfx_fill_rect(x + 2, y + 2, fill_w, inner_h, color);
    if (fill_w < inner_w) gfx_fill_rect(x + 2 + fill_w, y + 2, inner_w - fill_w, inner_h, bg);
}
