#include "accel.h"
#include "gpio.h"
#include "clock.h"
#include "log.h"
#include <string.h>
#include <stdio.h>

// Candidate SCL / SDA pin pairs for LP715_MB_V1.3 smartwatches
typedef struct {
    gpio_pin_e scl;
    gpio_pin_e sda;
    const char *desc;
} i2c_pin_pair_t;

static const i2c_pin_pair_t s_pairs[] = {
    // Pure candidates on LP715_MB_V1.3
    { GPIO_P16, GPIO_P17, "P16/P17" },
    { GPIO_P17, GPIO_P16, "P17/P16" },
    { GPIO_P14, GPIO_P15, "P14/P15" },
    { GPIO_P15, GPIO_P14, "P15/P14" },
};
#define NUM_PAIRS (sizeof(s_pairs) / sizeof(s_pairs[0]))

// Known G-Sensor I2C 7-bit addresses
static const uint8_t s_addrs[] = {
    0x12, // QMA6100P / QMA7981 (Primary)
    0x13, // QMA6100P (Alternate)
    0x19, // SC7A20 / LIS3DH (SDO=VCC)
    0x18, // SC7A20 / LIS3DH (SDO=GND)
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
    SENSOR_TYPE_QMA,   // QMA6100P / QMA7981
    SENSOR_TYPE_SC7A,  // SC7A20 / LIS3DH
    SENSOR_TYPE_HK01,  // HK01BS / BMA250
    SENSOR_TYPE_DA280, // DA280 / DA217
} sensor_type_t;

static bool s_detected = false;
static gpio_pin_e s_scl = GPIO_P14;
static gpio_pin_e s_sda = GPIO_P15;
static uint8_t s_i2c_addr = 0x12;
static uint8_t s_chip_id = 0x00;
static sensor_type_t s_sensor_type = SENSOR_TYPE_NONE;
static char s_sensor_name[16] = "NONE";
static const char *s_pin_desc = "NONE";

static int16_t s_acc_x = 0;
static int16_t s_acc_y = 0;
static int16_t s_acc_z = 0;

static inline void i2c_delay(void) {
    // 30 NOPs at 16 MHz = ~1.8 us (~140 kHz I2C clock)
    __asm__ volatile (
        "nop\n nop\n nop\n nop\n nop\n"
        "nop\n nop\n nop\n nop\n nop\n"
        "nop\n nop\n nop\n nop\n nop\n"
        "nop\n nop\n nop\n nop\n nop\n"
        "nop\n nop\n nop\n nop\n nop\n"
        "nop\n nop\n nop\n nop\n nop\n"
    );
}

static inline void i2c_scl_low(void) {
    hal_gpio_fast_write(s_scl, 0);
}

static inline void i2c_scl_high(void) {
    hal_gpio_fast_write(s_scl, 1);
}

static inline void i2c_sda_low(void) {
    hal_gpio_fast_write(s_sda, 0);
}

static inline void i2c_sda_high(void) {
    hal_gpio_fast_write(s_sda, 1);
}

static inline void i2c_sda_set_output(void) {
    hal_gpio_cfg_analog_io(s_sda, Bit_DISABLE);
    hal_gpio_pin_init(s_sda, GPIO_OUTPUT);
    hal_gpio_fast_write(s_sda, 1);
}

static inline void i2c_sda_set_input(void) {
    hal_gpio_pin_init(s_sda, GPIO_INPUT);
    hal_gpio_pull_set(s_sda, GPIO_PULL_UP_S);
}

static inline void i2c_scl_set_output(void) {
    hal_gpio_cfg_analog_io(s_scl, Bit_DISABLE);
    hal_gpio_pin_init(s_scl, GPIO_OUTPUT);
    hal_gpio_fast_write(s_scl, 1);
}

static inline void i2c_scl_set_input(void) {
    hal_gpio_pin_init(s_scl, GPIO_INPUT);
    hal_gpio_pull_set(s_scl, GPIO_PULL_UP_S);
}

static void i2c_start(void) {
    i2c_sda_set_output();
    i2c_scl_set_output();
    i2c_sda_high();
    i2c_delay();
    i2c_scl_high();
    i2c_delay();
    i2c_sda_low();
    i2c_delay();
    i2c_scl_low();
    i2c_delay();
}

static void i2c_stop(void) {
    i2c_sda_set_output();
    i2c_sda_low();
    i2c_delay();
    i2c_scl_high();
    i2c_delay();
    i2c_sda_high();
    i2c_delay();
}

static bool i2c_write_byte(uint8_t data) {
    i2c_sda_set_output();
    for (uint8_t mask = 0x80; mask != 0; mask >>= 1) {
        if (data & mask) i2c_sda_high();
        else i2c_sda_low();
        i2c_delay();
        i2c_scl_high();
        i2c_delay();
        i2c_scl_low();
        i2c_delay();
    }

    // 9th clock: Read ACK from slave
    i2c_sda_set_input();
    i2c_delay();
    i2c_scl_high();
    i2c_delay();
    bool ack = (hal_gpio_read(s_sda) == 0);
    i2c_scl_low();
    i2c_delay();
    i2c_sda_set_output();
    return ack;
}

static uint8_t i2c_read_byte(bool ack) {
    uint8_t data = 0;
    i2c_sda_set_input();

    for (uint8_t mask = 0x80; mask != 0; mask >>= 1) {
        i2c_delay();
        i2c_scl_high();
        i2c_delay();
        if (hal_gpio_read(s_sda)) data |= mask;
        i2c_scl_low();
        i2c_delay();
    }

    // 9th clock: Master sends ACK (LOW) or NACK (HIGH)
    i2c_sda_set_output();
    if (ack) i2c_sda_low();
    else i2c_sda_high();
    i2c_delay();
    i2c_scl_high();
    i2c_delay();
    i2c_scl_low();
    i2c_delay();
    return data;
}

static bool i2c_write_reg(uint8_t dev_addr, uint8_t reg_addr, uint8_t val) {
    i2c_start();
    if (!i2c_write_byte((dev_addr << 1) | 0)) {
        i2c_stop();
        return false;
    }
    if (!i2c_write_byte(reg_addr)) {
        i2c_stop();
        return false;
    }
    if (!i2c_write_byte(val)) {
        i2c_stop();
        return false;
    }
    i2c_stop();
    return true;
}

static bool i2c_read_reg(uint8_t dev_addr, uint8_t reg_addr, uint8_t *val) {
    i2c_start();
    if (!i2c_write_byte((dev_addr << 1) | 0)) {
        i2c_stop();
        return false; // NO ACK: device does not exist at this address
    }
    if (!i2c_write_byte(reg_addr)) {
        i2c_stop();
        return false;
    }
    i2c_start(); // repeated start
    if (!i2c_write_byte((dev_addr << 1) | 1)) {
        i2c_stop();
        return false;
    }
    *val = i2c_read_byte(false); // NACK on last byte
    i2c_stop();
    return true;
}

static void release_bus(void) {
    i2c_scl_set_input();
    i2c_sda_set_input();
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
    // Enable digital GPIO mode on P16/P17 (disconnect analog 32K crystal circuit)
    subWriteReg(&(AP_AON->PMCTL2_0), 6, 6, 0x01);
    WaitUs(50);
    hal_gpio_cfg_analog_io(GPIO_P16, Bit_DISABLE);
    hal_gpio_cfg_analog_io(GPIO_P17, Bit_DISABLE);

    // Power enable P02 (analog rail) and P07 (sensor VCC rail in factory ROM)
    hal_gpio_cfg_analog_io(GPIO_P02, Bit_DISABLE);
    hal_gpio_pin_init(GPIO_P02, GPIO_OUTPUT);
    hal_gpio_write(GPIO_P02, 1);

    hal_gpio_cfg_analog_io(GPIO_P07, Bit_DISABLE);
    hal_gpio_pin_init(GPIO_P07, GPIO_OUTPUT);
    hal_gpio_write(GPIO_P07, 1);

    // Set P18 (INT1) as high pull-up input
    hal_gpio_cfg_analog_io(GPIO_P18, Bit_DISABLE);
    hal_gpio_pin_init(GPIO_P18, GPIO_INPUT);
    hal_gpio_pull_set(GPIO_P18, GPIO_PULL_UP_S);

    WaitMs(30);

    LOG("[ACCEL] Starting auto-scan of %d pairs (P02=1, P07=1)...\n", (int)NUM_PAIRS);

    for (size_t p = 0; p < NUM_PAIRS; p++) {
        s_scl = s_pairs[p].scl;
        s_sda = s_pairs[p].sda;

        hal_gpio_cfg_analog_io(s_scl, Bit_DISABLE);
        hal_gpio_cfg_analog_io(s_sda, Bit_DISABLE);
        release_bus();
        WaitMs(1);

        // Verify bus idle state: both lines MUST float HIGH with pull-ups!
        if (hal_gpio_read(s_scl) == 0 || hal_gpio_read(s_sda) == 0) {
            LOG("[ACCEL] Pair %s stuck LOW (SCL=%d, SDA=%d), skip\n",
                s_pairs[p].desc, hal_gpio_read(s_scl), hal_gpio_read(s_sda));
            continue;
        }

        for (size_t a = 0; a < NUM_ADDRS; a++) {
            uint8_t dev_addr = s_addrs[a];

            // Ping test: send START + SLA+W, check ACK
            i2c_start();
            bool ack = i2c_write_byte((dev_addr << 1) | 0);
            i2c_stop();

            if (ack) {
                LOG("[ACCEL ACK!] Pair %s answered Addr=0x%02X!\n", s_pairs[p].desc, dev_addr);
                uint8_t val = 0;

                // Test 1: QMA6100P / QMA7981 (Reg 0x00 != 0 && != 0xFF)
                if (dev_addr == 0x12 || dev_addr == 0x13) {
                    i2c_write_reg(dev_addr, 0x11, 0x80); // soft reset
                    WaitMs(15);
                    if (i2c_read_reg(dev_addr, 0x00, &val) && val != 0x00 && val != 0xFF) {
                        LOG("[ACCEL] QMA CHIP_ID: 0x%02X on %s\n", val, s_pairs[p].desc);
                        s_detected = true;
                        s_i2c_addr = dev_addr;
                        s_chip_id = val;
                        s_pin_desc = s_pairs[p].desc;
                        s_sensor_type = SENSOR_TYPE_QMA;
                        if (val == 0xE8 || val == 0x01) strcpy(s_sensor_name, "QMA7981");
                        else if (val == 0x90 || val == 0x05) strcpy(s_sensor_name, "QMA6100");
                        else snprintf(s_sensor_name, sizeof(s_sensor_name), "QMA:%02X", val);

                        i2c_write_reg(dev_addr, 0x0F, 0x01); // 2g
                        i2c_write_reg(dev_addr, 0x10, 0x08); // 50Hz
                        i2c_write_reg(dev_addr, 0x11, 0x80); // active
                        return;
                    }
                }

                // Test 2: SC7A20 / LIS3DH (Reg 0x0F)
                if (i2c_read_reg(dev_addr, 0x0F, &val) && val != 0x00 && val != 0xFF) {
                    LOG("[ACCEL] SC7A20 WHO_AM_I: 0x%02X on %s\n", val, s_pairs[p].desc);
                    if (val == 0x11 || val == 0x33 || val == 0x26) {
                        s_detected = true;
                        s_sensor_type = SENSOR_TYPE_SC7A;
                        s_i2c_addr = dev_addr;
                        s_chip_id = val;
                        s_pin_desc = s_pairs[p].desc;
                        strcpy(s_sensor_name, "SC7A20");

                        i2c_write_reg(dev_addr, 0x20, 0x57); // 100Hz, Normal mode, XYZ enable
                        i2c_write_reg(dev_addr, 0x23, 0x08); // High resolution
                        return;
                    }
                }

                // Test 3: Generic Reg 0x00
                if (i2c_read_reg(dev_addr, 0x00, &val) && val != 0x00 && val != 0xFF) {
                    LOG("[ACCEL] Generic CHIP_ID: 0x%02X on %s\n", val, s_pairs[p].desc);
                    s_detected = true;
                    s_i2c_addr = dev_addr;
                    s_chip_id = val;
                    s_pin_desc = s_pairs[p].desc;
                    s_sensor_type = SENSOR_TYPE_HK01;
                    snprintf(s_sensor_name, sizeof(s_sensor_name), "ID:%02X", val);
                    return;
                }

                // Test 4: Reg 0x01 (DA280 / MSA301)
                if (i2c_read_reg(dev_addr, 0x01, &val) && (val == 0x13 || val == 0x26)) {
                    LOG("[ACCEL] DA280/MSA301 ID: 0x%02X on %s\n", val, s_pairs[p].desc);
                    s_detected = true;
                    s_i2c_addr = dev_addr;
                    s_chip_id = val;
                    s_pin_desc = s_pairs[p].desc;
                    s_sensor_type = SENSOR_TYPE_DA280;
                    strcpy(s_sensor_name, "DA280");
                    return;
                }
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
        if (i2c_write_byte((s_i2c_addr << 1) | 0) && i2c_write_byte(0x01)) {
            i2c_start();
            if (i2c_write_byte((s_i2c_addr << 1) | 1)) {
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
                return;
            }
        }
        i2c_stop();
    } else {
        // SC7A20 / LIS3DH: OUT_X_L (0x28 | 0x80 auto-increment)
        i2c_start();
        if (i2c_write_byte((s_i2c_addr << 1) | 0) && i2c_write_byte(0x28 | 0x80)) {
            i2c_start();
            if (i2c_write_byte((s_i2c_addr << 1) | 1)) {
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
                return;
            }
        }
        i2c_stop();
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
