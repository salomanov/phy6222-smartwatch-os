#include "accel.h"
#include "config.h"
#include "gpio.h"
#include "clock.h"
#include "log.h"
#include <string.h>

// Motion sensor driver for LP715_MB_V1.3
// Hardware: HK01BS / UM201 mechanical roll-ball vibration switch on GPIO_P18
// Reverse-engineered from factory firmware (dump 0x110126e8 / 0x11031954 / 0x1103377c)

static bool s_detected = false;
static const char *s_sensor_name = "HK01BS";

// Hardware ISR pulse counter
static volatile uint32_t s_raw_pulses = 0;
static uint32_t s_prev_pulses = 0;
static uint32_t s_step_count = 0;

// Factory filter parameters
static uint16_t s_window_pulses = 0;
static uint8_t  s_idle_ticks = 0;
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
    s_window_pulses = 0;
    s_idle_ticks = 0;
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

    LOG("[MOTION] HK01BS driver initialized on P18 with Edge ISR\n");
}

void accel_update(void) {
    if (!s_detected) return;

    uint32_t cur = s_raw_pulses;
    uint32_t delta = cur - s_prev_pulses;
    s_prev_pulses = cur;

    if (delta > 0) {
        s_idle_ticks = 0;
        s_window_pulses += (uint16_t)delta;

        // Factory algorithm (0x11016cf0): series threshold to filter out random isolated shocks
        if (!s_in_walking_session) {
            if (s_window_pulses >= 10) {
                // Confirmed walking session! Flush accumulated steps
                s_in_walking_session = true;
                s_step_count += (s_window_pulses / 2);
                s_window_pulses = 0;
            }
        } else {
            // Active walking session: each 2 ball state breaks count as 1 step
            if (s_window_pulses >= 2) {
                uint16_t add = s_window_pulses / 2;
                s_step_count += add;
                s_window_pulses %= 2;
            }
        }

        // Generate synthetic dynamic acceleration for watchface animations
        s_acc_x = (int16_t)(delta * 35);
        s_acc_y = (int16_t)(delta * 50);
        s_acc_z = (int16_t)(1000 + delta * 20);
    } else {
        // Decay synthetic values back to 1G stationary
        if (s_acc_x > 0) s_acc_x -= 5;
        if (s_acc_y > 0) s_acc_y -= 5;
        s_acc_z = 1000;

        // Inactivity timeout: if no motion for ~2 seconds (40 * 50ms ticks), reset walking session
        if (s_idle_ticks < 50) {
            s_idle_ticks++;
            if (s_idle_ticks >= 40) {
                s_in_walking_session = false;
                s_window_pulses = 0;
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
    s_window_pulses = 0;
    s_in_walking_session = false;
}
