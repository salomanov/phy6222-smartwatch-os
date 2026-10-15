#include "accel.h"
#include "gpio.h"
#include "clock.h"
#include "log.h"
#include <string.h>

// Candidate SCL / SDA pin pairs for LP715_MB_V1.3 / D20 / FitPro smartwatches
typedef struct {
    gpio_pin_e scl;
    gpio_pin_e sda;
    const char *desc;
} i2c_pin_pair_t;

static const i2c_pin_pair_t s_pairs[] = {
    { GPIO_P14, GPIO_P15, "P14/P15" },
    { GPIO_P15, GPIO_P14, "P15/P14" },
    { GPIO_P20, GPIO_P23, "P20/P23" },
    { GPIO_P23, GPIO_P20, "P23/P20" },
    { GPIO_P33, GPIO_P18, "P33/P18" },
    { GPIO_P18, GPIO_P33, "P18/P33" },
    { GPIO_P26, GPIO_P27, "P26/P27" },
    { GPIO_P27, GPIO_P26, "P27/P26" },
    { GPIO_P07, GPIO_P02, "P07/P02" },
    { GPIO_P02, GPIO_P07, "P02/P07" },
    { GPIO_P33, GPIO_P14, "P33/P14" },
    { GPIO_P14, GPIO_P33, "P14/P33" },
    { GPIO_P33, GPIO_P15, "P33/P15" },
    { GPIO_P15, GPIO_P33, "P15/P33" },
    { GPIO_P33, GPIO_P20, "P33/P20" },
    { GPIO_P20, GPIO_P33, "P20/P33" },
    { GPIO_P33, GPIO_P23, "P33/P23" },
    { GPIO_P23, GPIO_P33, "P23/P33" },
    { GPIO_P18, GPIO_P14, "P18/P14" },
    { GPIO_P14, GPIO_P18, "P14/P18" },
    { GPIO_P18, GPIO_P15, "P18/P15" },
    { GPIO_P15, GPIO_P18, "P15/P18" },
    { GPIO_P18, GPIO_P20, "P18/P20" },
    { GPIO_P20, GPIO_P18, "P20/P18" },
    { GPIO_P18, GPIO_P23, "P18/P23" },
    { GPIO_P23, GPIO_P18, "P23/P18" },
    { GPIO_P26, GPIO_P23, "P26/P23" },
    { GPIO_P23, GPIO_P26, "P26/P23" },
};
#define NUM_PAIRS (sizeof(s_pairs) / sizeof(s_pairs[0]))

// Known G-Sensor I2C 7-bit addresses
static const uint8_t s_addrs[] = {
    0x19, // SC7A20 / LIS3DH (SDO=VCC - standard in wristbands)
    0x18, // SC7A20 / LIS3DH (SDO=GND)
    0x12, // QMA6100P / QMA7981 / QMA6981 (Primary)
    0x13, // QMA6100P (Alternate)
    0x38, // HK01BS / BMA250
    0x26, // DA280 / DA217
    0x27, // DA280 Alternate
    0x0E, // MSA301 / KXTJ3
    0x0F, // MC3416
    0x68, // MPU6050
};
#define NUM_ADDRS (sizeof(s_addrs) / sizeof(s_addrs[0]))

typedef enum {
    SENSOR_TYPE_NONE = 0,
    SENSOR_TYPE_QMA,   // QMA6100P / QMA7981 / QMA6981
    SENSOR_TYPE_SC7A,  // SC7A20 / LIS3DH
    SENSOR_TYPE_HK01,  // HK01BS / BMA250
} sensor_type_t;

static bool s_detected = false;
static gpio_pin_e s_scl = GPIO_P14;
static gpio_pin_e s_sda = GPIO_P15;
static uint8_t s_i2c_addr = 0x19;
static uint8_t s_chip_id = 0x00;
static sensor_type_t s_sensor_type = SENSOR_TYPE_NONE;
static char s_sensor_name[16] = "NONE";
static const char *s_pin_desc = "NONE";

static int16_t s_acc_x = 0;
static int16_t s_acc_y = 0;
static int16_t s_acc_z = 0;

static inline void i2c_delay(void) {
    // 25 NOPs at 16 MHz = ~1.5 us, matching factory ROM bit-bang timing
    __asm__ volatile (
        "nop\n nop\n nop\n nop\n nop\n"
        "nop\n nop\n nop\n nop\n nop\n"
        "nop\n nop\n nop\n nop\n nop\n"
        "nop\n nop\n nop\n nop\n nop\n"
        "nop\n nop\n nop\n nop\n nop\n"
    );
}

static inline void i2c_sda_out(bool hi) {
    hal_gpio_pin_init(s_sda, GPIO_OUTPUT);
    hal_gpio_write(s_sda, hi ? 1 : 0);
}

static inline void i2c_sda_in_mode(void) {
    hal_gpio_pin_init(s_sda, GPIO_INPUT);
    hal_gpio_pull_set(s_sda, GPIO_PULL_UP_S);
}

static inline void i2c_scl_out(bool hi) {
    hal_gpio_pin_init(s_scl, GPIO_OUTPUT);
    hal_gpio_write(s_scl, hi ? 1 : 0);
}

static void i2c_start(void) {
    i2c_sda_out(1);
    i2c_delay();
    i2c_scl_out(1);
    i2c_delay();
    i2c_sda_out(0);
    i2c_delay();
    i2c_scl_out(0);
    i2c_delay();
}

static void i2c_stop(void) {
    i2c_sda_out(0);
    i2c_delay();
    i2c_scl_out(1);
    i2c_delay();
    i2c_sda_out(1);
    i2c_delay();
}

static bool i2c_write_byte(uint8_t byte) {
    for (int i = 7; i >= 0; i--) {
        i2c_sda_out((byte >> i) & 1);
        i2c_delay();
        i2c_scl_out(1);
        i2c_delay();
        i2c_scl_out(0);
        i2c_delay();
    }
    // Read ACK from slave (slave pulls SDA low)
    i2c_sda_in_mode();
    i2c_delay();
    i2c_scl_out(1);
    i2c_delay();
    bool ack = (hal_gpio_read(s_sda) == 0);
    i2c_scl_out(0);
    i2c_delay();
    i2c_sda_out(1); // restore output
    return ack;
}

static uint8_t i2c_read_byte(bool ack) {
    uint8_t byte = 0;
    i2c_sda_in_mode();
    for (int i = 7; i >= 0; i--) {
        i2c_scl_out(1);
        i2c_delay();
        if (hal_gpio_read(s_sda)) byte |= (1 << i);
        i2c_scl_out(0);
        i2c_delay();
    }
    // Send ACK (LOW) or NACK (HIGH)
    i2c_sda_out(!ack);
    i2c_delay();
    i2c_scl_out(1);
    i2c_delay();
    i2c_scl_out(0);
    i2c_delay();
    i2c_sda_out(1);
    return byte;
}

static bool i2c_ping(uint8_t addr) {
    i2c_start();
    bool ack = i2c_write_byte((addr << 1) | 0);
    i2c_stop();
    return ack;
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

static void release_bus(void) {
    hal_gpio_pin_init(s_scl, GPIO_INPUT);
    hal_gpio_pull_set(s_scl, GPIO_PULL_UP_S);
    hal_gpio_pin_init(s_sda, GPIO_INPUT);
    hal_gpio_pull_set(s_sda, GPIO_PULL_UP_S);
    WaitMs(1);
}

void accel_init(void) {
    s_detected = false;
    s_sensor_type = SENSOR_TYPE_NONE;
    strcpy(s_sensor_name, "SEARCH..");
    s_pin_desc = "NONE";

    // Power enable P02 (analog rail) and P07 (sensor VCC rail in factory ROM)
    hal_gpio_cfg_analog_io(GPIO_P02, Bit_DISABLE);
    hal_gpio_pin_init(GPIO_P02, GPIO_OUTPUT);
    hal_gpio_write(GPIO_P02, 1);

    hal_gpio_cfg_analog_io(GPIO_P07, Bit_DISABLE);
    hal_gpio_pin_init(GPIO_P07, GPIO_OUTPUT);
    hal_gpio_write(GPIO_P07, 1);
    WaitMs(30);

    LOG("[ACCEL] Starting auto-scan of %d pin pairs (P02=1, P07=1)...\n", (int)NUM_PAIRS);

    for (size_t p = 0; p < NUM_PAIRS; p++) {
        s_scl = s_pairs[p].scl;
        s_sda = s_pairs[p].sda;

        // Ensure digital IO
        hal_gpio_cfg_analog_io(s_scl, Bit_DISABLE);
        hal_gpio_cfg_analog_io(s_sda, Bit_DISABLE);
        hal_gpio_fmux(s_scl, Bit_DISABLE);
        hal_gpio_fmux(s_sda, Bit_DISABLE);
        release_bus();

        for (size_t a = 0; a < NUM_ADDRS; a++) {
            uint8_t dev_addr = s_addrs[a];
            uint8_t id_0f = 0, id_00 = 0;

            // Probe 1: SC7A20 / LIS3DH (Reg 0x0F WHO_AM_I)
            if (i2c_read_reg(dev_addr, 0x0F, &id_0f)) {
                if (id_0f == 0x11 || id_0f == 0x33 || id_0f == 0x26) {
                    s_detected = true;
                    s_sensor_type = SENSOR_TYPE_SC7A;
                    s_i2c_addr = dev_addr;
                    s_chip_id = id_0f;
                    s_pin_desc = s_pairs[p].desc;

                    // SC7A20 init: 50Hz, XYZ enable (CTRL_REG1: 0x57)
                    i2c_write_reg(dev_addr, 0x20, 0x57);
                    i2c_write_reg(dev_addr, 0x23, 0x08); // High resolution +-2g

                    strcpy(s_sensor_name, "SC7A20");
                    LOG("[ACCEL FOUND!] SC7A20 at %s, Addr=0x%02X, ID=0x%02X\n",
                        s_pin_desc, s_i2c_addr, s_chip_id);
                    return;
                }
            }

            // Probe 2: QMA6100P / QMA7981 (Reg 0x00 CHIP ID after soft-reset)
            if (dev_addr == 0x12 || dev_addr == 0x13) {
                i2c_write_reg(dev_addr, 0x11, 0x80); // soft reset
                WaitMs(10);
                i2c_read_reg(dev_addr, 0x00, &id_00);
            }

            if (id_00 != 0x00 && id_00 != 0xFF) {
                s_detected = true;
                s_i2c_addr = dev_addr;
                s_chip_id = id_00;
                s_pin_desc = s_pairs[p].desc;

                if (id_00 == 0x90 || id_00 == 0x05) {
                    s_sensor_type = SENSOR_TYPE_QMA;
                    strcpy(s_sensor_name, "QMA6100");
                    i2c_write_reg(dev_addr, 0x0F, 0x01);
                    i2c_write_reg(dev_addr, 0x10, 0x08);
                    i2c_write_reg(dev_addr, 0x11, 0x80);
                } else if (id_00 == 0xE8 || id_00 == 0x01) {
                    s_sensor_type = SENSOR_TYPE_QMA;
                    strcpy(s_sensor_name, "QMA7981");
                    i2c_write_reg(dev_addr, 0x0F, 0x01);
                    i2c_write_reg(dev_addr, 0x10, 0x08);
                    i2c_write_reg(dev_addr, 0x11, 0x80);
                } else if (id_00 == 0x03 || id_00 == 0x38 || id_00 == 0x13) {
                    s_sensor_type = SENSOR_TYPE_HK01;
                    strcpy(s_sensor_name, "HK01BS");
                } else {
                    s_sensor_type = SENSOR_TYPE_QMA;
                    snprintf(s_sensor_name, sizeof(s_sensor_name), "ID:%02X", id_00);
                }

                LOG("[ACCEL FOUND!] %s at %s, Addr=0x%02X, ID=0x%02X\n",
                    s_sensor_name, s_pin_desc, s_i2c_addr, s_chip_id);
                return;
            }
        }

        release_bus();
    }

    strcpy(s_sensor_name, "NONE");
    LOG("[ACCEL] No G-sensor detected on scanned pins.\n");
}

void accel_update(void) {
    if (!s_detected) return;

    if (s_sensor_type == SENSOR_TYPE_QMA) {
        // QMA: Output registers 0x01 (X_L), 0x02 (X_H), 0x03 (Y_L), 0x04 (Y_H), 0x05 (Z_L), 0x06 (Z_H)
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
        // SC7A20 / LIS3DH: OUT_X_L (0x28 | 0x80 auto-increment)
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

const char *accel_get_pins(void) {
    return s_pin_desc;
}

uint8_t accel_get_addr(void) {
    return s_i2c_addr;
}

uint8_t accel_get_chip_id(void) {
    return s_chip_id;
}
