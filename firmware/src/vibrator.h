#ifndef VIBRATOR_H
#define VIBRATOR_H

#include <stdint.h>

void vibrator_init(void);
void vibrator_pulse(uint16_t duration_ms);
void vibrator_double_pulse(void);
void vibrator_long_buzz(void);

// Status Green LED functions (GPIO_P00)
void led_init(void);
void led_set(uint8_t on);
void led_toggle(void);

// Pin test / scanner functions
uint8_t vibrator_get_test_pin(void);
const char *vibrator_get_test_pin_name(void);
void vibrator_next_test_pin(void);
void vibrator_prev_test_pin(void);
void vibrator_test_fire(void);
uint8_t vibrator_get_active_level(void);
void vibrator_toggle_active_level(void);
void vibrator_select_current_pin(void);

#endif // VIBRATOR_H
