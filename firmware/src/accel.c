#include "accel.h"
#include "config.h"
#include "gpio.h"
#include "clock.h"
#include "log.h"
#include <string.h>

// Motion sensor & pedometer driver for LP715_MB_V1.3
// Hardware: HK01BS / UM201 mechanical roll-ball vibration switch on GPIO_P18
// Reverse-engineered from factory firmware (dump 0x110126e8 / 0x11031954 / 0x1103377c)

static bool s_detected = false;
static const char *s_sensor_name = "HK01BS";

// Hardware ISR pulse counter
static volatile uint32_t s_raw_pulses = 0;
static uint32_t s_prev_pulses = 0;
static uint32_t s_step_count = 0;

// Factory windowing & cadence filter (evaluated every 500ms / 10 OSAL ticks)
#define CADENCE_WINDOW_TICKS 10 // 10 * 50ms = 500ms
static uint8_t  s_tick_acc = 0;
static uint16_t s_streak_pulses = 0;
static uint8_t  s_idle_windows = 0;
static bool     s_in_walking_session = false;

// Synthetic XYZ for UI / BLE compatibility
static int16_t s_acc_x = 0;
static int16_t s_acc_y = 0;
static int16_t s_acc_z = 1000; // 1G baseline

static void motion_edge_isr(gpio_pin_e pin, gpio_polarity_e type) {
    if (pin == ACCEL_INT_PIN) {
        s_raw_pulses++;
    }
}

void accel_init(void) {
    s_detected = true;
    s_raw_pulses = 0;
    s_prev_pulses = 0;
    s_step_count = 0;
    s_tick_acc = 0;
    s_streak_pulses = 0;
    s_idle_windows = 0;
    s_in_walking_session = false;

    // 1. Ensure sensor power rail (P15) is ON
    hal_gpio_cfg_analog_io(ACCEL_PWR_PIN, Bit_DISABLE);
    hal_gpio_pin_init(ACCEL_PWR_PIN, GPIO_OUTPUT);
    hal_gpio_write(ACCEL_PWR_PIN, 1);

    // 2. Configure P18 (HK01BS motion switch input)
    hal_gpio_cfg_analog_io(ACCEL_INT_PIN, Bit_DISABLE);
    hal_gpio_pin_init(ACCEL_INT_PIN, GPIO_INPUT);
    
    // Enable pull-up so that when the mechanical ball disconnects from GND, P18 rises cleanly to HIGH
    hal_gpio_pull_set(ACCEL_INT_PIN, GPIO_PULL_UP_S);

    // 3. Register hardware edge interrupt (both edges to capture fast chatter/bounce)
    hal_gpioin_register(ACCEL_INT_PIN, motion_edge_isr, motion_edge_isr);
}

void accel_update(void) {
    if (!s_detected) return;

    s_tick_acc++;
    if (s_tick_acc < CADENCE_WINDOW_TICKS) {
        return; // Evaluate every 500ms window
    }
    s_tick_acc = 0;

    uint32_t cur = s_raw_pulses;
    uint32_t delta = cur - s_prev_pulses;
    s_prev_pulses = cur;

    if (delta > 0) {
        s_idle_windows = 0;
        s_streak_pulses += (uint16_t)delta;

        // Factory algorithm (dump 0x11016cf0 & 0x11031954):
        // Filter out isolated random wrist twitches/desk vibrations.
        // Requires a cumulative streak of at least 15 pulses to confirm walking.
        if (!s_in_walking_session) {
            if (s_streak_pulses >= 15) {
                s_in_walking_session = true;
                // Credit the initial 2 steps that started the sequence
                s_step_count += 2;
            }
        }

        if (s_in_walking_session) {
            // Human cadence mapping per 500ms window:
            // - Normal walk (cadence ~1.5 - 2 steps/sec): 1-8 ball chattering pulses -> 1 step
            // - Brisk walk (cadence ~2 - 2.5 steps/sec): 9-16 pulses -> 2 steps
            // - Running/jogging: >16 pulses -> 3 steps
            if (delta <= 8) {
                s_step_count += 1;
            } else if (delta <= 16) {
                s_step_count += 2;
            } else {
                s_step_count += 3;
            }
        }

        // Dynamic synthetic acceleration for UI / watchface
        s_acc_x = (int16_t)(delta * 25);
        s_acc_y = (int16_t)(delta * 35);
        s_acc_z = (int16_t)(1000 + delta * 15);
    } else {
        // No motion in this 500ms window
        if (s_acc_x > 0) s_acc_x -= 10;
        if (s_acc_y > 0) s_acc_y -= 10;
        s_acc_z = 1000;

        // Inactivity timeout: 4 idle windows (~2 seconds) terminates walking session
        if (s_idle_windows < 10) {
            s_idle_windows++;
            if (s_idle_windows >= 4) {
                s_in_walking_session = false;
                s_streak_pulses = 0;
            }
        }
    }
}

bool accel_is_detected(void) {
    return s_detected;
}

void accel_get_xyz(int16_t *x, int16_t *y, int16_t *z) {
    if (x) *x = s_acc_x;
    if (y) *y = s_acc_y;
    if (z) *z = s_acc_z;
}

const char *accel_get_name(void) {
    return s_sensor_name;
}

uint32_t accel_get_steps(void) {
    return s_step_count;
}

uint32_t accel_get_pulses(void) {
    return s_raw_pulses;
}

void accel_reset_steps(void) {
    s_step_count = 0;
    s_raw_pulses = 0;
    s_prev_pulses = 0;
    s_streak_pulses = 0;
    s_tick_acc = 0;
    s_idle_windows = 0;
    s_in_walking_session = false;
}
