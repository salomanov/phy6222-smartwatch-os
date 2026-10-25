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
#define BUTTON_PIN            GPIO_P11    // Capacitive touch sensor
#define GPIO_VIBRATOR         GPIO_P03    // Vibration motor (Active HIGH: 1 = ON, 0 = OFF)
#define VIBRATOR_ACTIVE_LEVEL 1           // 1 = Active HIGH

// Accelerometer / Motion Sensor (I2C on P16/P17, Power on P15, INT on P18)
#define ACCEL_SCL_PIN         GPIO_P16    // I2C Clock (XTALI pin in digital mode)
#define ACCEL_SDA_PIN         GPIO_P17    // I2C Data (XTALO pin in digital mode)
#define ACCEL_PWR_PIN         GPIO_P15    // Power Enable / Reset
#define ACCEL_INT_PIN         GPIO_P18    // Motion Sensor INT1 input

// Optical Heart Rate Sensor (PPG on back cover)
#define HR_PWR_PIN            GPIO_P00    // Optical sensor power switch (Active LOW: 0 = ON)
#define HR_LED_PIN            GPIO_P14    // Green optical LED gate (Active HIGH: 1 = ON)

// UART
#define UART_TX_PIN   GPIO_P09    // UART TX -> CH340 RX
#define UART_RX_PIN   GPIO_P10    // UART RX -> CH340 TX

// Geometry
#define SCREEN_WIDTH  80
#define SCREEN_HEIGHT 160
#define ST7735_COLSTART 26
#define ST7735_ROWSTART 1

// Firmware Version
#define FW_VERSION_STR "v1.5.18-DIFF"

#endif // CONFIG_H
