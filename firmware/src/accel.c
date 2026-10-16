#include "accel.h"
#include "gpio.h"
#include "clock.h"
#include "log.h"
#include "mcu.h"
#include <string.h>

// Accelerometer auto-detection driver (Bit-Bang I2C)
// Dedicated hardware pins for LP715_MB_V1.3: P16 (SCL) and P17 (SDA)
static bool s_detected = false;
static const gpio_pin_e s_scl = GPIO_P16;
static const gpio_pin_e s_sda = GPIO_P17;
static uint8_t s_i2c_addr = 0x19;
static char s_sensor_name[16] = "NONE";

static int16_t s_acc_x = 0;
static int16_t s_acc_y = 0;
static int16_t s_acc_z = 0;

static void i2c_delay(void) {
    for (volatile int i = 0; i < 40; i++) {
        __asm__ volatile ("nop");
    }
}

static void i2c_sda_out(bool hi) {
    if (hi) {
        hal_gpio_pin_init(s_sda, GPIO_INPUT);
        hal_gpio_pull_set(s_sda, GPIO_PULL_UP_S);
    } else {
        hal_gpio_pin_init(s_sda, GPIO_OUTPUT);
        hal_gpio_write(s_sda, 0);
    }
}

static bool i2c_sda_in(void) {
    hal_gpio_pin_init(s_sda, GPIO_INPUT);
    hal_gpio_pull_set(s_sda, GPIO_PULL_UP_S);
    return hal_gpio_read(s_sda);
}

static void i2c_scl_out(bool hi) {
    if (hi) {
        hal_gpio_pin_init(s_scl, GPIO_INPUT);
        hal_gpio_pull_set(s_scl, GPIO_PULL_UP_S);
    } else {
        hal_gpio_pin_init(s_scl, GPIO_OUTPUT);
        hal_gpio_write(s_scl, 0);
    }
}

static void i2c_start(void) {
    i2c_sda_out(true);
    i2c_scl_out(true);
    i2c_delay();
    i2c_sda_out(false);
    i2c_delay();
    i2c_scl_out(false);
    i2c_delay();
}

static void i2c_stop(void) {
    i2c_sda_out(false);
    i2c_delay();
    i2c_scl_out(true);
    i2c_delay();
    i2c_sda_out(true);
    i2c_delay();
}

static bool i2c_write_byte(uint8_t byte) {
    for (int i = 7; i >= 0; i--) {
        i2c_sda_out((byte >> i) & 1);
        i2c_delay();
        i2c_scl_out(true);
        i2c_delay();
        i2c_scl_out(false);
        i2c_delay();
    }
    // Read ACK
    i2c_sda_out(true);
    i2c_delay();
    i2c_scl_out(true);
    i2c_delay();
    bool ack = !i2c_sda_in();
    i2c_scl_out(false);
    i2c_delay();
    return ack;
}

static uint8_t i2c_read_byte(bool ack) {
    uint8_t byte = 0;
    i2c_sda_out(true);
    for (int i = 7; i >= 0; i--) {
        i2c_scl_out(true);
        i2c_delay();
        if (i2c_sda_in()) byte |= (1 << i);
        i2c_scl_out(false);
        i2c_delay();
    }
    // Send ACK / NACK
    i2c_sda_out(!ack);
    i2c_delay();
    i2c_scl_out(true);
    i2c_delay();
    i2c_scl_out(false);
    i2c_delay();
    return byte;
}

static bool i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t val) {
    i2c_start();
    if (!i2c_write_byte((addr << 1) | 0)) { i2c_stop(); return false; }
    if (!i2c_write_byte(reg)) { i2c_stop(); return false; }
    if (!i2c_write_byte(val)) { i2c_stop(); return false; }
    i2c_stop();
    return true;
}

static bool i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *val) {
    i2c_start();
    if (!i2c_write_byte((addr << 1) | 0)) { i2c_stop(); return false; }
    if (!i2c_write_byte(reg)) { i2c_stop(); return false; }
    i2c_start(); // repeated start
    if (!i2c_write_byte((addr << 1) | 1)) { i2c_stop(); return false; }
    *val = i2c_read_byte(false);
    i2c_stop();
    return true;
}

void accel_init(void) {
    s_detected = false;
    strcpy(s_sensor_name, "SEARCH..");

    // 1. Switch P16/P17 from 32K analog crystal mode to digital GPIO mode
    subWriteReg(&(AP_AON->PMCTL2_0), 6, 6, 0x01);
    WaitMs(5);

    // 2. Configure P16 (SCL) and P17 (SDA) as inputs with strong pull-up
    hal_gpio_cfg_analog_io(s_scl, Bit_DISABLE);
    hal_gpio_pin_init(s_scl, GPIO_INPUT);
    hal_gpio_pull_set(s_scl, GPIO_PULL_UP_S);

    hal_gpio_cfg_analog_io(s_sda, Bit_DISABLE);
    hal_gpio_pin_init(s_sda, GPIO_INPUT);
    hal_gpio_pull_set(s_sda, GPIO_PULL_UP_S);
    WaitMs(5);

    int sda_st = hal_gpio_read(s_sda);
    LOG("[ACCEL] Hardware pins P16/P17 initialized, SDA state: %d\n", sda_st);

    // Common G-sensor addresses: 0x19 (SC7A20 factory 0x32), 0x18, 0x12 (QMA6100), 0x13, 0x38 (HK01BS)
    static const uint8_t candidate_addrs[] = { 0x19, 0x18, 0x12, 0x13, 0x38, 0x26, 0x68 };
    for (size_t i = 0; i < sizeof(candidate_addrs); i++) {
        uint8_t a = candidate_addrs[i];
        i2c_start();
        bool ack = i2c_write_byte((a << 1) | 0);
        i2c_stop();

        if (ack) {
            uint8_t id0 = 0, idf = 0;
            i2c_read_reg(a, 0x00, &id0);
            i2c_read_reg(a, 0x0F, &idf);
            LOG("[I2C-ACK] Device at 0x%02X! reg0=0x%02X regF=0x%02X\n", a, id0, idf);

            if (idf == 0x11 || idf == 0x26 || idf == 0x33 || a == 0x18 || a == 0x19) {
                s_detected = true;
                s_i2c_addr = a;
                strcpy(s_sensor_name, "SC7A20");
                i2c_write_reg(a, 0x20, 0x57); // 50Hz, XYZ enable
                i2c_write_reg(a, 0x23, 0x88); // High-Res, BDU
                LOG("[ACCEL] SC7A20 configured on P16/P17\n");
                return;
            }

            if ((id0 >= 0x90 && id0 <= 0xB9) || a == 0x12) {
                s_detected = true;
                s_i2c_addr = a;
                strcpy(s_sensor_name, "QMA6100");
                i2c_write_reg(a, 0x0F, 0x01);
                i2c_write_reg(a, 0x10, 0x08);
                i2c_write_reg(a, 0x11, 0x80);
                LOG("[ACCEL] QMA6100 configured on P16/P17\n");
                return;
            }

            s_detected = true;
            s_i2c_addr = a;
            strcpy(s_sensor_name, "G-SENS");
            return;
        }
    }

    strcpy(s_sensor_name, "NO ACK");
    LOG("[ACCEL] P16/P17 scan complete: no ACK received\n");
}

void accel_update(void) {
    if (!s_detected) return;

    if (s_i2c_addr == 0x12 || s_i2c_addr == 0x13) {
        // QMA6100 / QMA7981
        i2c_start();
        if (!i2c_write_byte((s_i2c_addr << 1) | 0)) { i2c_stop(); return; }
        if (!i2c_write_byte(0x01)) { i2c_stop(); return; }
        i2c_start();
        if (!i2c_write_byte((s_i2c_addr << 1) | 1)) { i2c_stop(); return; }

        uint8_t xl = i2c_read_byte(true);
        uint8_t xh = i2c_read_byte(true);
        uint8_t yl = i2c_read_byte(true);
        uint8_t yh = i2c_read_byte(true);
        uint8_t zl = i2c_read_byte(true);
        uint8_t zh = i2c_read_byte(false);
        i2c_stop();

        s_acc_x = (int16_t)((xh << 8) | xl) / 64;
        s_acc_y = (int16_t)((yh << 8) | yl) / 64;
        s_acc_z = (int16_t)((zh << 8) | zl) / 64;
    } else {
        // SC7A20 / HK01BS
        i2c_start();
        if (!i2c_write_byte((s_i2c_addr << 1) | 0)) { i2c_stop(); return; }
        if (!i2c_write_byte(0x28 | 0x80)) { i2c_stop(); return; }
        i2c_start();
        if (!i2c_write_byte((s_i2c_addr << 1) | 1)) { i2c_stop(); return; }

        uint8_t xl = i2c_read_byte(true);
        uint8_t xh = i2c_read_byte(true);
        uint8_t yl = i2c_read_byte(true);
        uint8_t yh = i2c_read_byte(true);
        uint8_t zl = i2c_read_byte(true);
        uint8_t zh = i2c_read_byte(false);
        i2c_stop();

        s_acc_x = (int16_t)((xh << 8) | xl) / 64;
        s_acc_y = (int16_t)((yh << 8) | yl) / 64;
        s_acc_z = (int16_t)((zh << 8) | zl) / 64;
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
