#include "vibrator.h"
#include "config.h"
#include "gpio.h"
#include "mcu.h"

void vibrator_init(void) {
    hal_gpio_pin_init(GPIO_VIBRATOR, GPIO_OUTPUT);
    hal_gpio_write(GPIO_VIBRATOR, 0);
}

void vibrator_pulse(uint16_t duration_ms) {
    hal_gpio_write(GPIO_VIBRATOR, 1);
    WaitMs(duration_ms);
    hal_gpio_write(GPIO_VIBRATOR, 0);
}

void vibrator_double_pulse(void) {
    vibrator_pulse(40);
    WaitMs(60);
    vibrator_pulse(40);
}

void vibrator_long_buzz(void) {
    vibrator_pulse(200);
}
