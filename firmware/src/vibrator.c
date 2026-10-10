#include "vibrator.h"
#include "config.h"
#include "gpio.h"

// Safe cycle-based microsecond delay for Cortex-M0 at 16MHz (~16 cycles per microsecond)
static void safe_delay_ms(uint32_t ms) {
    while (ms--) {
        for (volatile uint32_t i = 0; i < 2000; i++) {
            __asm__ volatile ("nop");
        }
    }
}

void vibrator_init(void) {
    hal_gpio_pin_init(GPIO_VIBRATOR, GPIO_OUTPUT);
    hal_gpio_write(GPIO_VIBRATOR, 0);
}

void vibrator_pulse(uint16_t duration_ms) {
    hal_gpio_write(GPIO_VIBRATOR, 1);
    safe_delay_ms(duration_ms);
    hal_gpio_write(GPIO_VIBRATOR, 0);
}

void vibrator_double_pulse(void) {
    vibrator_pulse(40);
    safe_delay_ms(60);
    vibrator_pulse(40);
}

void vibrator_long_buzz(void) {
    vibrator_pulse(150);
}

