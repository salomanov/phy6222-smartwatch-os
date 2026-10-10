#include "button.h"
#include "config.h"
#include "gpio.h"

#define DEBOUNCE_MS     30
#define LONG_PRESS_MS   700
#define DOUBLE_CLICK_MS 350

static bool s_last_state = false;
static uint32_t s_press_time = 0;
static uint32_t s_release_time = 0;
static bool s_waiting_double = false;
static bool s_long_press_fired = false;

void button_init(void) {
    hal_gpio_pin_init(BUTTON_PIN, GPIO_INPUT);
    hal_gpio_pull_set(BUTTON_PIN, GPIO_PULL_DOWN);
}

bool button_is_down(void) {
    return hal_gpio_read(BUTTON_PIN) != 0;
}

button_event_t button_update(uint32_t current_ms) {
    bool raw = button_is_down();
    button_event_t event = BTN_EVENT_NONE;

    if (raw && !s_last_state) {
        // Press detected
        s_press_time = current_ms;
        s_long_press_fired = false;
        s_last_state = true;
    } else if (raw && s_last_state) {
        // Still held down
        if (!s_long_press_fired && (current_ms - s_press_time >= LONG_PRESS_MS)) {
            s_long_press_fired = true;
            s_waiting_double = false;
            return BTN_EVENT_LONG_PRESS;
        }
    } else if (!raw && s_last_state) {
        // Released
        s_last_state = false;
        uint32_t duration = current_ms - s_press_time;

        if (!s_long_press_fired && duration >= DEBOUNCE_MS) {
            if (s_waiting_double && (current_ms - s_release_time <= DOUBLE_CLICK_MS)) {
                s_waiting_double = false;
                return BTN_EVENT_DOUBLE_CLICK;
            } else {
                s_waiting_double = true;
                s_release_time = current_ms;
            }
        }
    }

    if (s_waiting_double && (current_ms - s_release_time > DOUBLE_CLICK_MS)) {
        s_waiting_double = false;
        return BTN_EVENT_CLICK;
    }

    return event;
}
