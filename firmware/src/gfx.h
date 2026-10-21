#ifndef GFX_H
#define GFX_H

#include <stdint.h>
#include <stdbool.h>
#include "display.h"

// Basic Drawing Primitives
void gfx_draw_pixel(int16_t x, int16_t y, uint16_t color);
void gfx_draw_fast_h_line(int16_t x, int16_t y, int16_t w, uint16_t color);
void gfx_draw_fast_v_line(int16_t x, int16_t y, int16_t h, uint16_t color);
void gfx_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
void gfx_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void gfx_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void gfx_draw_circle(int16_t x0, int16_t y0, int16_t r, uint16_t color);
void gfx_fill_circle(int16_t x0, int16_t y0, int16_t r, uint16_t color);
void gfx_draw_round_rect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color);
void gfx_fill_round_rect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color);

// Text & Character Rendering (5x7 Font with Cyrillic & Scaling)
void gfx_draw_char(int16_t x, int16_t y, uint8_t c, uint16_t color, uint16_t bg, uint8_t size);
void gfx_draw_string(int16_t x, int16_t y, const char *str, uint16_t color, uint16_t bg, uint8_t size);

// UTF-8 Helper to render both Latin and Russian UTF-8 strings
void gfx_draw_string_utf8(int16_t x, int16_t y, const char *utf8_str, uint16_t color, uint16_t bg, uint8_t size);

// UI Widget Helpers
void gfx_draw_battery_icon(int16_t x, int16_t y, uint8_t percent, bool charging);
void gfx_draw_ble_icon(int16_t x, int16_t y, bool connected);
void gfx_draw_heart_icon(int16_t x, int16_t y, uint16_t color);
void gfx_draw_step_icon(int16_t x, int16_t y, uint16_t color);
void gfx_draw_bitmap_1bpp(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color, uint16_t bg);
void gfx_draw_progress_bar(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t percent, uint16_t color, uint16_t bg);

#endif // GFX_H
