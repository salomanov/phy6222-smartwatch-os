#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

// ST7735 / ST7789 Command Definitions
#define ST77XX_NOP        0x00
#define ST77XX_SWRESET    0x01
#define ST77XX_SLPIN      0x10
#define ST77XX_SLPOUT     0x11
#define ST77XX_NORON      0x13
#define ST77XX_INVOFF     0x20
#define ST77XX_INVON      0x21
#define ST77XX_DISPOFF    0x28
#define ST77XX_DISPON     0x29
#define ST77XX_CASET      0x2A
#define ST77XX_RASET      0x2B
#define ST77XX_RAMWR      0x2C
#define ST77XX_MADCTL     0x36
#define ST77XX_COLMOD     0x3A

#define ST7735_FRMCTR1    0xB1
#define ST7735_FRMCTR2    0xB2
#define ST7735_FRMCTR3    0xB3
#define ST7735_INVCTR     0xB4
#define ST7735_PWCTR1     0xC0
#define ST7735_PWCTR2     0xC1
#define ST7735_PWCTR3     0xC2
#define ST7735_PWCTR4     0xC3
#define ST7735_PWCTR5     0xC4
#define ST7735_VMCTR1     0xC5
#define ST7735_GMCTRP1    0xE0
#define ST7735_GMCTRN1    0xE1

// MADCTL Flags
#define ST77XX_MADCTL_MY  0x80
#define ST77XX_MADCTL_MX  0x40
#define ST77XX_MADCTL_MV  0x20
#define ST77XX_MADCTL_ML  0x10
#define ST77XX_MADCTL_RGB 0x00
#define ST7735_MADCTL_BGR 0x08

// RGB565 Colors
#define COLOR_BLACK       0x0000
#define COLOR_WHITE       0xFFFF
#define COLOR_RED         0xF800
#define COLOR_GREEN       0x07E0
#define COLOR_BLUE        0x001F
#define COLOR_CYAN        0x07FF
#define COLOR_MAGENTA     0xF81F
#define COLOR_YELLOW      0xFFE0
#define COLOR_ORANGE      0xFD20
#define COLOR_DARKGREY    0x39E7
#define COLOR_LIGHTGREY   0xC618
#define COLOR_STEALTH_DIM 0x2104

// Functions
void display_init(uint8_t rotation);
void display_set_rotation(uint8_t rotation);
void display_set_addr_window(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1);
void display_fill_screen(uint16_t color);
void display_fill_window(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint16_t* color_buffer, uint32_t size);
void display_draw_pixel(uint16_t x, uint16_t y, uint16_t color);
uint16_t display_rgb(uint8_t r, uint8_t g, uint8_t b);

void backlight_on(void);
void backlight_off(void);

uint16_t display_get_width(void);
uint16_t display_get_height(void);

#endif // DISPLAY_H
