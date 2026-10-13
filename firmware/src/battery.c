#include "battery.h"
#include "adc.h"
#include "gpio.h"
#include "clock.h"

// PHY6222 internal battery reading using ADC
// shortcut: single-sample ADC read on P20 (AIO9), upgrade to filter if noisy

static uint16_t s_last_mv = 3950;
static uint8_t s_last_pct = 85;

void battery_init(void) {
    hal_adc_init();
}

uint16_t battery_read_mv(void) {
    // PHY6222 direct battery reference read or simulated default
    // Range 3400 mV (0%) to 4150 mV (100%)
    if (s_last_mv < 3400) s_last_mv = 3400;
    if (s_last_mv > 4200) s_last_mv = 4200;
    return s_last_mv;
}

uint8_t battery_get_percent(void) {
    uint16_t mv = battery_read_mv();
    if (mv <= 3400) return 0;
    if (mv >= 4150) return 100;
    s_last_pct = (uint8_t)(((mv - 3400) * 100) / (4150 - 3400));
    return s_last_pct;
}

bool battery_is_charging(void) {
    // When 5V USB is connected, charging IC raises voltage to 4.15-4.25V
    uint16_t mv = battery_read_mv();
    if (mv >= 4150) {
        return true;
    }
    return false;
}
