#include "vibrator.h"
#include "config.h"
#include "gpio.h"
#include "clock.h"

// Safe cycle-based microsecond delay for Cortex-M0 at 16MHz
static void safe_delay_ms(uint32_t ms) {
    while (ms--) {
        for (volatile uint32_t i = 0; i < 2000; i++) {
            __asm__ volatile ("nop");
        }
    }
}

// Candidates for LP715 / PHY6222 board
typedef struct {
    gpio_pin_e pin;
    const char *name;
} test_pin_entry_t;

static const test_pin_entry_t s_test_pins[] = {
    { GPIO_P14, "P14 (Spec)" },
    { GPIO_P03, "P03"       },
    { GPIO_P02, "P02 (Ana)" },
    { GPIO_P00, "P00 (Led)" },
    { GPIO_P07, "P07"       },
    { GPIO_P15, "P15"       },
    { GPIO_P18, "P18"       },
    { GPIO_P20, "P20"       },
    { GPIO_P23, "P23"       },
    { GPIO_P26, "P26"       },
};

#define TEST_PINS_COUNT (sizeof(s_test_pins) / sizeof(s_test_pins[0]))
static uint8_t s_curr_idx = 0;
static uint8_t s_active_level = 1; // 1 = Active HIGH, 0 = Active LOW
static gpio_pin_e s_selected_pin = GPIO_P14;

static void setup_pin(gpio_pin_e pin) {
    hal_gpioretention_register(pin);
    hal_gpio_pin_init(pin, GPIO_OUTPUT);
    if (pin == GPIO_P02 || pin == GPIO_P03) {
        hal_gpio_pin2pin3_control(pin, 1);
        hal_gpio_cfg_analog_io(pin, Bit_DISABLE);
    }
    // Set inactive level
    hal_gpio_write(pin, s_active_level ? 0 : 1);
}

void vibrator_init(void) {
    s_selected_pin = GPIO_VIBRATOR;
    setup_pin(s_selected_pin);
}

void vibrator_pulse(uint16_t duration_ms) {
    if (duration_ms < 100) duration_ms = 100;
    setup_pin(s_selected_pin);
    hal_gpio_write(s_selected_pin, s_active_level ? 1 : 0);
    safe_delay_ms(duration_ms);
    hal_gpio_write(s_selected_pin, s_active_level ? 0 : 1);
}

void vibrator_double_pulse(void) {
    vibrator_pulse(60);
    safe_delay_ms(60);
    vibrator_pulse(60);
}

void vibrator_long_buzz(void) {
    vibrator_pulse(200);
}

uint8_t vibrator_get_test_pin(void) {
    return (uint8_t)s_test_pins[s_curr_idx].pin;
}

const char *vibrator_get_test_pin_name(void) {
    return s_test_pins[s_curr_idx].name;
}

void vibrator_next_test_pin(void) {
    s_curr_idx = (s_curr_idx + 1) % TEST_PINS_COUNT;
    setup_pin(s_test_pins[s_curr_idx].pin);
}

void vibrator_prev_test_pin(void) {
    if (s_curr_idx == 0) {
        s_curr_idx = TEST_PINS_COUNT - 1;
    } else {
        s_curr_idx--;
    }
    setup_pin(s_test_pins[s_curr_idx].pin);
}

uint8_t vibrator_get_active_level(void) {
    return s_active_level;
}

void vibrator_toggle_active_level(void) {
    s_active_level = s_active_level ? 0 : 1;
    setup_pin(s_test_pins[s_curr_idx].pin);
}

void vibrator_select_current_pin(void) {
    s_selected_pin = s_test_pins[s_curr_idx].pin;
}

void vibrator_test_fire(void) {
    gpio_pin_e pin = s_test_pins[s_curr_idx].pin;
    setup_pin(pin);
    // Fire strong 350ms pulse
    hal_gpio_write(pin, s_active_level ? 1 : 0);
    safe_delay_ms(350);
    hal_gpio_write(pin, s_active_level ? 0 : 1);
}
