#include "vibrator.h"
#include "config.h"
#include "gpio.h"
#include "clock.h"

// Strictly protected hardware pins:
// - GPIO_P07: Hardware TM (Test Mode) programming pin. Driving HIGH causes ROM bootloader entry.
// - GPIO_P33: ST7735 display line. Driving it causes screen blackout/bus collision.
static inline bool is_unsafe_pin(gpio_pin_e pin) {
    return (pin == GPIO_P07 || pin == GPIO_P33);
}

// Candidates for LP715 / PHY6222 board (P07 and P33 are strictly excluded)
// name: short label drawn at scale 2 (max 3 chars -> 36 px, fits the 80 px screen)
// tag:  scale-1 role hint drawn beside it (max 5 chars -> 30 px), "" = none
typedef struct {
    gpio_pin_e pin;
    const char *name;
    const char *tag;
} test_pin_entry_t;

static const test_pin_entry_t s_test_pins[] = {
    { GPIO_P03, "P03", "Vib" },
    { GPIO_P00, "P00", "Led" },
    { GPIO_P14, "P14", ""    },
    { GPIO_P02, "P02", "Ana" },
    { GPIO_P15, "P15", ""    },
    { GPIO_P18, "P18", ""    },
    { GPIO_P20, "P20", ""    },
    { GPIO_P23, "P23", ""    },
    { GPIO_P26, "P26", ""    },
};

#define TEST_PINS_COUNT (sizeof(s_test_pins) / sizeof(s_test_pins[0]))
static uint8_t s_curr_idx = 0; // P03 by default
static uint8_t s_active_level = VIBRATOR_ACTIVE_LEVEL; // 0 = Active LOW (0 turns on vib)
static gpio_pin_e s_selected_pin = GPIO_VIBRATOR;      // GPIO_P03

static uint16_t s_vib_remaining_ms = 0;
static bool s_vib_active = false;

static void setup_pin(gpio_pin_e pin) {
    if (is_unsafe_pin(pin)) {
        return; // Guard: NEVER configure or drive unsafe pins as OUTPUT!
    }
    hal_gpioretention_register(pin);
    hal_gpio_pin_init(pin, GPIO_OUTPUT);
    if (pin == GPIO_P02 || pin == GPIO_P03) {
        hal_gpio_pin2pin3_control(pin, 1);
        hal_gpio_cfg_analog_io(pin, Bit_DISABLE);
    }
    // Set inactive level
    hal_gpio_write(pin, s_active_level ? 0 : 1);
}

// Green Optical LED Status (HR_PWR_PIN active LOW, HR_LED_PIN active HIGH)
static uint8_t s_led_state = 0;

void led_init(void) {
    hal_gpio_cfg_analog_io(HR_PWR_PIN, Bit_DISABLE);
    hal_gpio_pin_init(HR_PWR_PIN, GPIO_OUTPUT);
    hal_gpio_cfg_analog_io(HR_LED_PIN, Bit_DISABLE);
    hal_gpio_pin_init(HR_LED_PIN, GPIO_OUTPUT);
    led_set(0); // Off by default
}

void led_set(uint8_t on) {
    s_led_state = on ? 1 : 0;
    if (s_led_state) {
        hal_gpio_write(HR_PWR_PIN, 0); // Power ON optical circuit (P00 active LOW)
        hal_gpio_write(HR_LED_PIN, 1); // Gate ON green LED (P14 active HIGH)
    } else {
        hal_gpio_write(HR_LED_PIN, 0); // Gate OFF green LED
        hal_gpio_write(HR_PWR_PIN, 1); // Power OFF optical circuit
    }
}

void led_toggle(void) {
    led_set(!s_led_state);
}

void vibrator_init(void) {
    s_selected_pin = GPIO_VIBRATOR;
    s_active_level = VIBRATOR_ACTIVE_LEVEL;
    s_vib_remaining_ms = 0;
    s_vib_active = false;
    setup_pin(s_selected_pin);
    led_init();
}

void vibrator_update(uint16_t elapsed_ms) {
    if (!s_vib_active) {
        return;
    }
    if (s_vib_remaining_ms <= elapsed_ms) {
        s_vib_remaining_ms = 0;
        s_vib_active = false;
        if (!is_unsafe_pin(s_selected_pin)) {
            hal_gpio_write(s_selected_pin, s_active_level ? 0 : 1);
        }
        led_set(0);
    } else {
        s_vib_remaining_ms -= elapsed_ms;
    }
}

void vibrator_pulse(uint16_t duration_ms) {
    if (is_unsafe_pin(s_selected_pin)) {
        return;
    }
    if (duration_ms < 80) duration_ms = 80;   // Minimum 80ms for physical ERM rotor acceleration
    if (duration_ms > 350) duration_ms = 350; // Safety ceiling
    setup_pin(s_selected_pin);
    led_set(1);
    hal_gpio_write(s_selected_pin, s_active_level ? 1 : 0);
    s_vib_remaining_ms = duration_ms;
    s_vib_active = true;
}

void vibrator_double_pulse(void) {
    vibrator_pulse(150);
}

void vibrator_long_buzz(void) {
    vibrator_pulse(250);
}

void vibrator_stop(void) {
    s_vib_remaining_ms = 0;
    s_vib_active = false;
    if (!is_unsafe_pin(s_selected_pin)) {
        hal_gpio_write(s_selected_pin, s_active_level ? 0 : 1);
    }
    led_set(0);
}

uint8_t vibrator_get_test_pin(void) {
    return (uint8_t)s_test_pins[s_curr_idx].pin;
}

const char *vibrator_get_test_pin_name(void) {
    return s_test_pins[s_curr_idx].name;
}

const char *vibrator_get_test_pin_tag(void) {
    return s_test_pins[s_curr_idx].tag;
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
    if (is_unsafe_pin(pin)) {
        return;
    }
    s_selected_pin = pin;
    setup_pin(s_selected_pin);
    led_set(1);
    // Non-blocking 350ms pulse
    hal_gpio_write(s_selected_pin, s_active_level ? 1 : 0);
    s_vib_remaining_ms = 350;
    s_vib_active = true;
}

