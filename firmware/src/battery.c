#include "battery.h"
#include "adc.h"
#include "gpio.h"
#include "clock.h"
#include "log.h"
#include "mcu.h"

// PHY6222 internal battery reading on P20 (AIO9 / ADC_CH3P_P20)
// LP715 hardware has a 1:2 resistor divider to battery Li-Po (3.4V - 4.2V).

static volatile uint16_t s_raw_adc_mv = 0;
static volatile uint16_t s_bat_mv = 3900;
static volatile bool s_sampling = false;

static void on_adc_evt(adc_Evt_t *pev) {
    if (pev->type == HAL_ADC_EVT_DATA && pev->ch == ADC_CH3P_P20) {
        float v = hal_adc_value_cal((adc_CH_t)pev->ch, pev->data, pev->size, FALSE, FALSE);
        uint16_t adc_mv = (uint16_t)(v * 1000.0f);
        uint16_t bat_mv = (uint16_t)(v * 2000.0f); // 1:2 external voltage divider

        s_raw_adc_mv = adc_mv;
        if (bat_mv >= 2800 && bat_mv <= 4500) {
            // Apply lightweight filter
            s_bat_mv = (s_bat_mv * 3 + bat_mv) / 4;
        }

        hal_adc_stop();
        s_sampling = false;
        LOG("[ADC] P20 raw=%dmV bat=%dmV\n", adc_mv, s_bat_mv);
    }
}

void battery_init(void) {
    hal_adc_init();
    battery_sample_now();
}

void battery_sample_now(void) {
    if (s_sampling) return;
    s_sampling = true;

    adc_Cfg_t cfg = {
        .channel = ADC_BIT(ADC_CH3P_P20),
        .is_continue_mode = FALSE,
        .is_differential_mode = 0x00,
        .is_high_resolution = 0x00,
    };

    int ret = hal_adc_config_channel(cfg, on_adc_evt);
    if (ret != PPlus_SUCCESS) {
        s_sampling = false;
        return;
    }
    hal_adc_start();
}

uint16_t battery_get_raw_adc_mv(void) {
    return s_raw_adc_mv;
}

uint16_t battery_read_mv(void) {
    if (s_bat_mv < 3000) return 3000;
    if (s_bat_mv > 4300) return 4300;
    return s_bat_mv;
}

uint8_t battery_get_percent(void) {
    uint16_t mv = battery_read_mv();
    if (mv <= 3400) return 0;
    if (mv >= 4180) return 100;
    return (uint8_t)(((mv - 3400) * 100) / (4180 - 3400));
}

bool battery_is_charging(void) {
    return (battery_read_mv() >= 4180);
}
