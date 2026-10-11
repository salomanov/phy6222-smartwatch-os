#ifndef CONFIG_H
#define CONFIG_H

#include "gpio.h"

// Hardware Pin Configuration for LP715_MB_V1.3 (PHY6222QC)
// Display ST7735S (0.96" 80x160 IPS SPI)
#define DC_PIN        GPIO_P25
#define RST_PIN       GPIO_P24
#define CS_PIN        GPIO_P31
#define BKL_PIN       GPIO_P01    // Active LOW: 0 = Backlight ON, 1 = Backlight OFF
#define SCLK_PIN      GPIO_P34
#define MOSI_PIN      GPIO_P32

// Peripherals
#define BUTTON_PIN    GPIO_P11    // Capacitive touch sensor
#define GPIO_VIBRATOR GPIO_P14    // Vibration motor (hardware pin P14, active HIGH)
#define GPIO_LED      GPIO_P00    // Status LED

// UART
#define UART_TX_PIN   GPIO_P09    // UART TX -> CH340 RX
#define UART_RX_PIN   GPIO_P10    // UART RX -> CH340 TX

// Geometry
#define SCREEN_WIDTH  80
#define SCREEN_HEIGHT 160
#define ST7735_COLSTART 26
#define ST7735_ROWSTART 1

#endif // CONFIG_H
