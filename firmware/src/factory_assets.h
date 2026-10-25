#ifndef FACTORY_ASSETS_H
#define FACTORY_ASSETS_H

#include <stdint.h>
#include <stdbool.h>

// XIP addresses in factory flash bank 0x11072000 (preserved above OTA staging)
#define FACTORY_SPRITE_BAT_BASE   0x11072000U // 5 battery icons (16x8, stride 26)
#define FACTORY_SPRITE_BLE_ADDR   0x11072082U // 1 BLE icon (16x7, 24 bytes)
#define FACTORY_SPRITE_SMDIG_BASE 0x1107209AU // 14 small 8x10 digits (stride 20)
#define FACTORY_SPRITE_DIV_ADDR   0x110721B2U // 24x2 divider (16 bytes)
#define FACTORY_SPRITE_BGDIG_BASE 0x110721C2U // 11 giant 16x29 digits 0..9 and ':' (stride 68)
#define FACTORY_SPRITE_WKDAY_BASE 0x110724AEU // 7 weekday 32x10 badges MON..SUN (stride 50)
#define FACTORY_BMP_WALLPAPER     0x1107260CU // 80x160 8-bit indexed BMP wallpaper (13876 bytes)

// RLE Color Icon IDs from factory asset pack (0x1105886C)
#define FAC_RLE_STEPS_SHOE     37 // 50x36

// 1bpp Monochrome Icon IDs from factory asset pack (0x1105886C)
#define FAC_MONO_ALARM         10 // 37x41
#define FAC_MONO_TROPHY        11 // 41x41

// Render full-screen 80x160 8bpp BMP wallpaper from 0x1107260C
void factory_draw_bmp_wallpaper(void);

// Render 1bpp factory sprite at (x, y); if over_bmp is true, '0' bits sample the 0x1107260C wallpaper
void factory_draw_sprite_1bpp(uint32_t flash_addr, uint8_t x, uint8_t y, uint16_t fg_color, uint16_t bg_color, bool over_bmp);

// Draw big 16x29 HH:MM time at y position
void factory_draw_big_time(uint8_t hours, uint8_t minutes, uint8_t y, uint16_t fg_color, bool over_bmp);

// Draw small 8x10 numeric string (0-9, '/', ':', '-') at (x, y)
void factory_draw_small_num(uint8_t x, uint8_t y, const char *str, uint16_t fg_color, uint16_t bg_color, bool over_bmp);

// Render proportional 16px UTF-8 text (ASCII + Cyrillic) from factory font 0x1103F000
int16_t factory_draw_utf8(int16_t x, int16_t y, const char *utf8, uint16_t fg_color, uint16_t bg_color);

// Draw RLE color icon from factory asset pack (0x1105886C)
void factory_draw_rle_icon(uint8_t icon_id, uint8_t frame, uint8_t x, uint8_t y);

// Draw 1bpp monochrome icon from factory asset pack (0x1105886C)
void factory_draw_mono_icon(uint8_t icon_id, uint8_t x, uint8_t y, uint16_t fg_color, uint16_t bg_color);

#endif // FACTORY_ASSETS_H
