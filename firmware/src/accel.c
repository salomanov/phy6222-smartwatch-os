#include "accel.h"
#include "gpio.h"
#include "clock.h"
#include <string.h>

// Accelerometer auto-detection driver (Bit-Bang I2C)
// Common candidate pin pairs for LP715/D20 boards
typedef struct {
    gpio_pin_e scl;
    gpio_pin_e sda;
    const char *desc;
} i2c_pin_pair_t;

static const i2c_pin_pair_t s_pairs[] = {
    { GPIO_P16, GPIO_P17, "P16/P17" }
};
#define NUM_PAIRS (sizeof(s_pairs) / sizeof(s_pairs[0]))

// Common 7-bit I2C addresses for G-Sensors (SC7A20/LIS3DH, HK01BS, DA280, BMA250)
static const uint8_t s_addrs[] = { 0x18, 0x19, 0x38, 0x26, 0x27, 0x68 };
#define NUM_ADDRS (sizeof(s_addrs) / sizeof(s_addrs[0]))

static bool s_detected = false;
static gpio_pin_e s_scl = GPIO_P16;
static gpio_pin_e s_sda = GPIO_P17;
static uint8_t s_i2c_addr = 0x18;
static char s_sensor_name[16] = "NONE";

static int16_t s_acc_x = 0;
static int16_t s_acc_y = 0;
static int16_t s_acc_z = 0;

static void i2c_delay(void) {
    for (volatile int i = 0; i < 20; i++) {
        __asm__ volatile ("nop");
    }
}

static void i2c_sda_out(bool hi) {
    if (hi) {
        hal_gpio_pin_init(s_sda, GPIO_INPUT);
        hal_gpio_pull_set(s_sda, GPIO_PULL_UP);
    } else {
        hal_gpio_pin_init(s_sda, GPIO_OUTPUT);
        hal_gpio_write(s_sda, 0);
    }
}

static bool i2c_sda_in(void) {
    hal_gpio_pin_init(s_sda, GPIO_INPUT);
    hal_gpio_pull_set(s_sda, GPIO_PULL_UP);
    return hal_gpio_read(s_sda);
}

static void i2c_scl_out(bool hi) {
    if (hi) {
        hal_gpio_pin_init(s_scl, GPIO_INPUT);
        hal_gpio_pull_set(s_scl, GPIO_PULL_UP);
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

    // Scan pin pairs and addresses
    for (size_t p = 0; p < NUM_PAIRS; p++) {
        s_scl = s_pairs[p].scl;
        s_sda = s_pairs[p].sda;

        for (size_t a = 0; a < NUM_ADDRS; a++) {
            uint8_t dev_addr = s_addrs[a];
            uint8_t id = 0;

            // Try reading WHO_AM_I (0x0F for SC7A20/LIS3DH, 0x00 for HK01/BMA)
            if (i2c_read_reg(dev_addr, 0x0F, &id) || i2c_read_reg(dev_addr, 0x00, &id)) {
                if (id != 0x00 && id != 0xFF) {
                    s_detected = true;
                    s_i2c_addr = dev_addr;

                    // Configure normal mode 50Hz, XYZ enable (CTRL_REG1: 0x57)
                    i2c_write_reg(dev_addr, 0x20, 0x57);

                    if (id == 0x11 || id == 0x33) {
                        strcpy(s_sensor_name, "SC7A20");
                    } else if (id == 0x38 || id == 0x03) {
                        strcpy(s_sensor_name, "HK01BS");
                    } else {
                        strcpy(s_sensor_name, "G-SENS");
                    }
                    return;
                }
            }
        }
    }

    strcpy(s_sensor_name, "IDLE");
}

void accel_update(void) {
    if (!s_detected) return;

    // Read 6 bytes of acceleration: OUT_X_L (0x28 | 0x80 auto-increment)
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
