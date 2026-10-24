#include "uart_cli.h"
#include "config.h"
#include "uart.h"
#include "gpio.h"
#include "log.h"
#include "mcu.h"
#include "clock.h"
#include "app_os.h"
#include "ble_manager.h"
#include "vibrator.h"
#include "display.h"
#include "accel.h"
#include "battery.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define CLI_RX_BUF_SIZE 128
static char s_rx_buf[CLI_RX_BUF_SIZE];
static volatile uint8_t s_rx_len = 0;
static volatile bool s_cmd_ready = false;

extern void app_os_set_time(uint8_t h, uint8_t m, uint8_t s);

static int cli_parse_u32(const char *s, int base, uint32_t *out, int max_n) {
    int count = 0;
    while (*s && count < max_n) {
        while (*s == ' ' || *s == '\t' || *s == ':') s++;
        if (!*s) break;
        if (base == 16 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
        uint32_t v = 0;
        bool got = false;
        while (*s) {
            char c = *s;
            int d = -1;
            if (c >= '0' && c <= '9') d = c - '0';
            else if (base == 16 && c >= 'a' && c <= 'f') d = c - 'a' + 10;
            else if (base == 16 && c >= 'A' && c <= 'F') d = c - 'A' + 10;
            if (d < 0 || d >= base) break;
            v = v * (uint32_t)base + (uint32_t)d;
            got = true;
            s++;
        }
        if (!got) break;
        out[count++] = v;
    }
    return count;
}

static gpio_pin_e pin_num_to_enum(int pin) {
    switch (pin) {
        case 0:  return GPIO_P00;
        case 1:  return GPIO_P01;
        case 2:  return GPIO_P02;
        case 3:  return GPIO_P03;
        case 7:  return GPIO_P07;
        case 9:  return GPIO_P09;
        case 10: return GPIO_P10;
        case 11: return GPIO_P11;
        case 14: return GPIO_P14;
        case 15: return GPIO_P15;
        case 16: return GPIO_P16;
        case 17: return GPIO_P17;
        case 18: return GPIO_P18;
        case 20: return GPIO_P20;
        case 23: return GPIO_P23;
        case 24: return GPIO_P24;
        case 25: return GPIO_P25;
        case 26: return GPIO_P26;
        case 27: return GPIO_P27;
        case 31: return GPIO_P31;
        case 32: return GPIO_P32;
        case 33: return GPIO_P33;
        case 34: return GPIO_P34;
        default: return GPIO_DUMMY;
    }
}

static void uart_send_str(const char *str) {
    hal_uart_send_buff(UART0, (uint8_t *)str, strlen(str));
}

// Low-level Bit-Bang I2C helper for CLI commands on P16/P17 (ACCEL Push-Pull mode)
static void cli_i2c_delay(void) {
    WaitUs(15);
}

static void cli_i2c_sda(bool hi) {
    hal_gpio_cfg_analog_io(GPIO_P17, Bit_DISABLE);
    hal_gpio_pin_init(GPIO_P17, GPIO_OUTPUT);
    hal_gpio_fast_write(GPIO_P17, hi ? 1 : 0);
}

static void cli_i2c_scl(bool hi) {
    hal_gpio_cfg_analog_io(GPIO_P16, Bit_DISABLE);
    hal_gpio_pin_init(GPIO_P16, GPIO_OUTPUT);
    hal_gpio_fast_write(GPIO_P16, hi ? 1 : 0);
}

static void cli_i2c_start(void) {
    cli_i2c_sda(true);
    cli_i2c_scl(true);
    cli_i2c_delay();
    cli_i2c_sda(false);
    cli_i2c_delay();
    cli_i2c_scl(false);
    cli_i2c_delay();
}

static void cli_i2c_stop(void) {
    cli_i2c_sda(false);
    cli_i2c_delay();
    cli_i2c_scl(true);
    cli_i2c_delay();
    cli_i2c_sda(true);
    cli_i2c_delay();
}

static bool cli_i2c_write_byte(uint8_t byte) {
    for (int i = 7; i >= 0; i--) {
        cli_i2c_sda((byte >> i) & 1);
        cli_i2c_delay();
        cli_i2c_scl(true);
        cli_i2c_delay();
        cli_i2c_scl(false);
        cli_i2c_delay();
    }
    // Switch SDA to input with hardware strong pull-up (10b = 2) to sample ACK
    hal_gpio_cfg_analog_io(GPIO_P17, Bit_DISABLE);
    hal_gpio_pin_init(GPIO_P17, GPIO_INPUT);
    subWriteReg(&(AP_AON->IOCTL[1]), 23, 22, 2); // 10b: 10k strong pull-up
    WaitUs(10);
    cli_i2c_scl(true);
    WaitUs(15);
    bool ack = (hal_gpio_read(GPIO_P17) == 0); // 0 = ACK (slave pulled line low)
    cli_i2c_scl(false);
    cli_i2c_delay();
    cli_i2c_sda(true);
    return ack;
}

static uint8_t cli_i2c_read_byte(bool ack) {
    uint8_t byte = 0;
    hal_gpio_cfg_analog_io(GPIO_P17, Bit_DISABLE);
    hal_gpio_pin_init(GPIO_P17, GPIO_INPUT);
    subWriteReg(&(AP_AON->IOCTL[1]), 23, 22, 2); // 10b: 10k strong pull-up
    WaitUs(10);
    for (int i = 7; i >= 0; i--) {
        cli_i2c_scl(true);
        WaitUs(15);
        if (hal_gpio_read(GPIO_P17)) byte |= (1 << i);
        cli_i2c_scl(false);
        cli_i2c_delay();
    }
    cli_i2c_sda(!ack);
    cli_i2c_delay();
    cli_i2c_scl(true);
    cli_i2c_delay();
    cli_i2c_scl(false);
    cli_i2c_delay();
    cli_i2c_sda(true);
    return byte;
}

static bool cli_i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *val) {
    cli_i2c_start();
    if (!cli_i2c_write_byte((addr << 1) | 0)) { cli_i2c_stop(); return false; }
    if (!cli_i2c_write_byte(reg)) { cli_i2c_stop(); return false; }
    cli_i2c_start();
    if (!cli_i2c_write_byte((addr << 1) | 1)) { cli_i2c_stop(); return false; }
    *val = cli_i2c_read_byte(false);
    cli_i2c_stop();
    return true;
}

static bool cli_i2c_bus_recover(void) {
    subWriteReg(&(AP_AON->IOCTL[1]), 23, 22, 2); // P17 strong pull-up
    subWriteReg(&(AP_AON->IOCTL[1]), 20, 19, 2); // P16 strong pull-up
    cli_i2c_scl(true);
    hal_gpio_cfg_analog_io(GPIO_P17, Bit_DISABLE);
    hal_gpio_pin_init(GPIO_P17, GPIO_INPUT);
    WaitUs(50);

    for (int i = 0; i < 9; i++) {
        cli_i2c_scl(false);
        WaitUs(25);
        cli_i2c_scl(true);
        WaitUs(25);
    }
    cli_i2c_sda(false);
    WaitUs(25);
    cli_i2c_scl(true);
    WaitUs(25);
    cli_i2c_sda(true);
    WaitUs(50);

    hal_gpio_cfg_analog_io(GPIO_P17, Bit_DISABLE);
    hal_gpio_pin_init(GPIO_P17, GPIO_INPUT);
    subWriteReg(&(AP_AON->IOCTL[1]), 23, 22, 2); // 10b: 10k strong pull-up
    WaitUs(50);
    return hal_gpio_read(GPIO_P17);
}

static void on_uart_evt(uart_Evt_t *pev) {
    if (pev->type == UART_EVT_TYPE_RX_DATA || pev->type == UART_EVT_TYPE_RX_DATA_TO) {
        for (uint16_t i = 0; i < pev->len; i++) {
            char c = (char)pev->data[i];
            if (c == '\r' || c == '\n') {
                if (s_rx_len > 0) {
                    s_rx_buf[s_rx_len] = '\0';
                    s_cmd_ready = true;
                }
            } else if (s_rx_len < CLI_RX_BUF_SIZE - 1) {
                s_rx_buf[s_rx_len++] = c;
            }
        }
    }
}

void uart_cli_init(void) {
    hal_uart_deinit(UART0);

    uart_Cfg_t cfg = {
        .tx_pin = P9,
        .rx_pin = P10,
        .rts_pin = GPIO_DUMMY,
        .cts_pin = GPIO_DUMMY,
        .baudrate = 115200,
        .use_fifo = TRUE,
        .hw_fwctrl = FALSE,
        .use_tx_buf = FALSE,
        .parity = FALSE,
        .evt_handler = on_uart_evt,
    };
    hal_uart_init(cfg, UART0);
    uart_send_str("\r\n=== CHASY-STEALTH UART INTERACTIVE CLI ===\r\nType 'help' for commands.\r\ncmd>> ");
}

void uart_cli_update(void) {
    if (!s_cmd_ready) return;

    char cmd[CLI_RX_BUF_SIZE];
    strcpy(cmd, s_rx_buf);
    s_rx_len = 0;
    s_cmd_ready = false;

    char resp[128];

    // Trim trailing whitespace
    int len = strlen(cmd);
    while (len > 0 && (cmd[len-1] == ' ' || cmd[len-1] == '\t')) {
        cmd[--len] = '\0';
    }

    if (strcmp(cmd, "ping") == 0) {
        uart_send_str("pong\r\n");
    } else if (strcmp(cmd, "help") == 0) {
        uart_send_str("\r\n=== CLI ===\r\n"
                      "  pin <p> <0|1> - Set pin\r\n"
                      "  read <p>      - Read pin\r\n"
                      "  bkl <1|0>     - Backlight\r\n"
                      "  disp          - Redraw\r\n"
                      "  vib [ms]      - Vibrate\r\n"
                      "  vib_test [ms] - Vib test P18\r\n"
                      "  motion        - Motion status\r\n"
                      "  step reset    - Reset steps\r\n"
                      "  i2c scan      - I2C scan\r\n"
                      "  i2c r <a> <r> - Read I2C\r\n"
                      "  i2c w <a><r><v>- Write I2C\r\n"
                      "  status        - Status\r\n"
                      "  adc           - Battery ADC\r\n"
                      "  time HH:MM:SS - Set time\r\n"
                      "  ota           - OTA reboot\r\n"
                      "  reboot        - Reset\r\n");
    } else if (strncmp(cmd, "pin ", 4) == 0) {
        uint32_t args[2] = {0, 0};
        if (cli_parse_u32(cmd + 4, 10, args, 2) == 2 && args[0] <= 34) {
            int pin = (int)args[0], val = (int)args[1];
            gpio_pin_e epin = pin_num_to_enum(pin);
            if (epin == GPIO_DUMMY) {
                snprintf(resp, sizeof(resp), "[ERR] Pin P%d does not exist on PHY6222.\r\n", pin);
            } else if (pin == 7) {
                snprintf(resp, sizeof(resp), "[ERR] Pin P07 is hardware TM input! Driving OUTPUT is forbidden.\r\n");
            } else if (pin == 9 || pin == 10) {
                snprintf(resp, sizeof(resp), "[ERR] Pins P09/P10 are UART0 TX/RX! Modifying them is forbidden.\r\n");
            } else if (pin == 20) {
                snprintf(resp, sizeof(resp), "[ERR] Pin P20 is battery ADC input! Driving OUTPUT is forbidden.\r\n");
            } else if (pin == 33 || pin == 24 || pin == 25 || pin == 31 || pin == 32 || pin == 34) {
                snprintf(resp, sizeof(resp), "[WARN] Pin P%d is reserved for ST7735 display! Skipped.\r\n", pin);
            } else {
                if (pin == 2 || pin == 3) {
                    hal_gpio_pin2pin3_control(epin, 1);
                }
                if (epin == GPIO_P16 || epin == GPIO_P17) {
                    subWriteReg(0x4000f014, 31, 27, 0x05);
                    subWriteReg(0x4000f01c, 16, 7, 0x3fb);
                    subWriteReg(0x4000f01c, 6, 6, 0x01);
                }
                hal_gpio_cfg_analog_io(epin, Bit_DISABLE);
                hal_gpio_pin_init(epin, GPIO_OUTPUT);
                hal_gpio_write(epin, val ? 1 : 0);
                snprintf(resp, sizeof(resp), "[OK] Pin P%d set to %d\r\n", pin, val ? 1 : 0);
            }
            uart_send_str(resp);
        } else {
            uart_send_str("Usage: pin <pin_num> <0|1>\r\n");
        }
    } else if (strncmp(cmd, "read ", 5) == 0) {
        uint32_t args[1] = {0};
        if (cli_parse_u32(cmd + 5, 10, args, 1) == 1 && args[0] <= 34) {
            int pin = (int)args[0];
            gpio_pin_e epin = pin_num_to_enum(pin);
            if (epin == GPIO_DUMMY) {
                snprintf(resp, sizeof(resp), "[ERR] Pin P%d does not exist on PHY6222.\r\n", pin);
                uart_send_str(resp);
            } else if (pin == 9 || pin == 10) {
                snprintf(resp, sizeof(resp), "[ERR] Pins P09/P10 are UART0 TX/RX! Cannot read as standard GPIO.\r\n");
                uart_send_str(resp);
            } else if (pin == 20) {
                snprintf(resp, sizeof(resp), "[WARN] Pin P20 is analog battery ADC input (use 'status' for bat).\r\n");
                uart_send_str(resp);
            } else {
                if (pin == 2 || pin == 3) {
                    hal_gpio_pin2pin3_control(epin, 1);
                }
                if (epin == GPIO_P16 || epin == GPIO_P17) {
                    subWriteReg(0x4000f014, 31, 27, 0x05);
                    subWriteReg(0x4000f01c, 16, 7, 0x3fb);
                    subWriteReg(0x4000f01c, 6, 6, 0x01);
                }
                hal_gpio_cfg_analog_io(epin, Bit_DISABLE);
                hal_gpio_pin_init(epin, GPIO_INPUT);
                if (epin == GPIO_P16) {
                    subWriteReg(&(AP_AON->IOCTL[1]), 20, 19, 2);
                } else if (epin == GPIO_P17) {
                    subWriteReg(&(AP_AON->IOCTL[1]), 23, 22, 2);
                } else {
                    hal_gpio_pull_set(epin, GPIO_PULL_UP_S);
                }
                WaitUs(50);
                int level = hal_gpio_read(epin);
                snprintf(resp, sizeof(resp), "[OK] Pin P%d = %d\r\n", pin, level);
                uart_send_str(resp);
            }
        } else {
            uart_send_str("Usage: read <pin_num>\r\n");
        }
    } else if (strncmp(cmd, "bkl ", 4) == 0) {
        uint32_t args[1] = {0};
        if (cli_parse_u32(cmd + 4, 10, args, 1) == 1) {
            if (args[0]) {
                backlight_on();
                uart_send_str("[OK] Backlight ON\r\n");
            } else {
                backlight_off();
                uart_send_str("[OK] Backlight OFF\r\n");
            }
        }
    } else if (strcmp(cmd, "disp") == 0) {
        display_init(0);
        app_os_force_redraw();
        uart_send_str("[OK] Display re-initialized.\r\n");
    } else if (strncmp(cmd, "vib_test", 8) == 0) {
        uint32_t args[1] = {120};
        cli_parse_u32(cmd + 8, 10, args, 1);
        int ms = (int)args[0];
        if (ms < 50 || ms > 1000) ms = 120;
        uint32_t p0 = accel_get_pulses();
        vibrator_pulse((uint16_t)ms);
        WaitMs(ms + 30);
        uint32_t p1 = accel_get_pulses();
        snprintf(resp, sizeof(resp), "[OK] Vib test %d ms: pulses before=%lu, after=%lu, caught=%lu\r\n",
                 ms, (unsigned long)p0, (unsigned long)p1, (unsigned long)(p1 - p0));
        uart_send_str(resp);
    } else if (strncmp(cmd, "vib", 3) == 0) {
        uint32_t args[1] = {80};
        cli_parse_u32(cmd + 3, 10, args, 1);
        int ms = (int)args[0];
        if (ms <= 0 || ms > 2000) ms = 80;
        vibrator_pulse((uint32_t)ms);
        snprintf(resp, sizeof(resp), "[OK] Vibration pulse %d ms\r\n", ms);
        uart_send_str(resp);
    } else if (strcmp(cmd, "motion") == 0) {
        snprintf(resp, sizeof(resp), "Motion (HK01BS): pulses=%lu, steps=%lu, P18=%d\r\n",
                 (unsigned long)accel_get_pulses(), (unsigned long)accel_get_steps(), hal_gpio_read(ACCEL_INT_PIN));
        uart_send_str(resp);
    } else if (strcmp(cmd, "step reset") == 0) {
        accel_reset_steps();
        uart_send_str("[OK] Step counters reset to 0\r\n");
    } else if (strcmp(cmd, "i2c scan") == 0) {
        uart_send_str("I2C init P16/P17...\r\n");
        
        // 1. Power on P02 (Sensor VCC power switch, factory 0x110312ba)
        hal_gpio_pin2pin3_control(GPIO_P02, 1);
        hal_gpio_cfg_analog_io(GPIO_P02, Bit_DISABLE);
        hal_gpio_pin_init(GPIO_P02, GPIO_OUTPUT);
        hal_gpio_write(GPIO_P02, 1);

        // Power-cycle sensors via P15 (Active HIGH)
        hal_gpio_cfg_analog_io(GPIO_P15, Bit_DISABLE);
        hal_gpio_pin_init(GPIO_P15, GPIO_OUTPUT);
        hal_gpio_write(GPIO_P15, 0);
        WaitMs(50);
        hal_gpio_write(GPIO_P15, 1);
        WaitMs(150);

        // 2. Power down 32K XTAL analog oscillator so P16/P17 become digital GPIOs
        subWriteReg(0x4000f014, 31, 27, 0x05); // PMCTL0: RCOSC 32K mode
        subWriteReg(0x4000f01c, 16, 7, 0x3fb); // PMCTL2_0: power down crystal
        subWriteReg(0x4000f01c, 6, 6, 0x01);   // PMCTL2_0: enable software control
        hal_gpio_cfg_analog_io(GPIO_P16, Bit_DISABLE);
        hal_gpio_cfg_analog_io(GPIO_P17, Bit_DISABLE);
        subWriteReg(&(AP_AON->IOCTL[1]), 23, 22, 2); // 10b: P17 strong pull-up
        subWriteReg(&(AP_AON->IOCTL[1]), 20, 19, 2); // 10b: P16 strong pull-up
        WaitMs(10);

        // 3. 9-clock bus recovery & verify SDA is idle HIGH
        bool sda_idle = cli_i2c_bus_recover();
        if (!sda_idle) {
            uart_send_str("[ERR] SDA line (P17) is stuck LOW! Bus recovery failed.\r\n"
                          "      Line not rising to 3.3V (missing pull-up or short to GND).\r\n");
            return;
        }
        uart_send_str("[OK] Bus recovery passed. SDA is idle HIGH (3.3V). Scanning 0x08..0x77:\r\n");

        int count = 0;
        for (uint8_t a = 0x08; a <= 0x77; a++) {
            cli_i2c_start();
            bool ack = cli_i2c_write_byte((a << 1) | 0);
            cli_i2c_stop();
            if (ack) {
                count++;
                uint8_t id = 0;
                if (a == 0x19 && cli_i2c_read_reg(a, 0x0F, &id)) {
                    snprintf(resp, sizeof(resp), "  -> Found 0x19: Silan SC7A20 Accelerometer! WHO_AM_I=0x%02X\r\n", id);
                } else if ((a == 0x12 || a == 0x13) && cli_i2c_read_reg(a, 0x00, &id)) {
                    snprintf(resp, sizeof(resp), "  -> Found 0x%02X: QST QMA6100/7981 Accelerometer! CHIP_ID=0x%02X\r\n", a, id);
                } else if (a == 0x44 && cli_i2c_read_reg(a, 0x00, &id)) {
                    snprintf(resp, sizeof(resp), "  -> Found 0x44: Elan EM7028 PPG Pulse Sensor! ID=0x%02X\r\n", id);
                } else {
                    snprintf(resp, sizeof(resp), "  -> Found ACK at 7-bit addr: 0x%02X (8-bit: 0x%02X)\r\n", a, a << 1);
                }
                uart_send_str(resp);
            }
        }
        snprintf(resp, sizeof(resp), "Scan complete. Found %d responsive device(s).\r\n", count);
        uart_send_str(resp);
    } else if (strncmp(cmd, "i2c r ", 6) == 0) {
        uint32_t args[2] = {0, 0};
        if (cli_parse_u32(cmd + 6, 16, args, 2) == 2) {
            unsigned int a = args[0], reg = args[1];
            uint8_t val = 0;
            if (cli_i2c_read_reg((uint8_t)a, (uint8_t)reg, &val)) {
                snprintf(resp, sizeof(resp), "[OK] Addr 0x%02X Reg 0x%02X = 0x%02X (%d)\r\n", a, reg, val, val);
                uart_send_str(resp);
            } else {
                uart_send_str("[ERR] NAK or read failure\r\n");
            }
        } else {
            uart_send_str("Usage: i2c r <hex_addr> <hex_reg> (e.g. i2c r 19 0f)\r\n");
        }
    } else if (strncmp(cmd, "peek ", 5) == 0) {
        uint32_t args[1] = {0};
        if (cli_parse_u32(cmd + 5, 16, args, 1) == 1) {
            uint32_t addr = args[0];
            uint32_t val = *(volatile uint32_t *)addr;
            snprintf(resp, sizeof(resp), "[PEEK] 0x%08X = 0x%08X\r\n", (unsigned int)addr, (unsigned int)val);
            uart_send_str(resp);
        }
    } else if (strncmp(cmd, "poke ", 5) == 0) {
        uint32_t args[2] = {0, 0};
        if (cli_parse_u32(cmd + 5, 16, args, 2) == 2) {
            uint32_t addr = args[0], val = args[1];
            *(volatile uint32_t *)addr = val;
            snprintf(resp, sizeof(resp), "[POKE] 0x%08X = 0x%08X\r\n", (unsigned int)addr, (unsigned int)val);
            uart_send_str(resp);
        }
    } else if (strncmp(cmd, "i2c w ", 6) == 0) {
        uint32_t args[3] = {0, 0, 0};
        if (cli_parse_u32(cmd + 6, 16, args, 3) == 3) {
            unsigned int a = args[0], reg = args[1], val = args[2];
            cli_i2c_start();
            if (!cli_i2c_write_byte((a << 1) | 0) || !cli_i2c_write_byte(reg) || !cli_i2c_write_byte(val)) {
                cli_i2c_stop();
                uart_send_str("[ERR] NAK during write\r\n");
            } else {
                cli_i2c_stop();
                snprintf(resp, sizeof(resp), "[OK] Addr 0x%02X Reg 0x%02X set to 0x%02X\r\n", a, reg, val);
                uart_send_str(resp);
            }
        } else {
            uart_send_str("Usage: i2c w <hex_addr> <hex_reg> <hex_val>\r\n");
        }
    } else if (strcmp(cmd, "status") == 0) {
        battery_sample_now();
        WaitMs(50);
        snprintf(resp, sizeof(resp), "FW: %s, BLE: %s, Bat: %d%% (%dmV, %s)\r\n",
                 FW_VERSION_STR,
                 ble_is_connected() ? "CONN" : "ADV",
                 battery_get_percent(), battery_read_mv(),
                 battery_is_charging() ? "CHG" : "DISCHG");
        uart_send_str(resp);
    } else if (strcmp(cmd, "adc") == 0) {
        battery_sample_now();
        WaitMs(60);
        snprintf(resp, sizeof(resp), "[ADC] P20 Raw: %dmV, Bat: %dmV (%d%%), %s\r\n",
                 battery_get_raw_adc_mv(),
                 battery_read_mv(),
                 battery_get_percent(),
                 battery_is_charging() ? "CHG" : "DISCHG");
        uart_send_str(resp);
    } else if (strncmp(cmd, "time ", 5) == 0) {
        uint32_t args[3] = {0, 0, 0};
        if (cli_parse_u32(cmd + 5, 10, args, 3) >= 2) {
            app_os_set_time((uint8_t)args[0], (uint8_t)args[1], (uint8_t)args[2]);
            snprintf(resp, sizeof(resp), "Time set to %02d:%02d:%02d\r\n", (int)args[0], (int)args[1], (int)args[2]);
            uart_send_str(resp);
        }
    } else if (strcmp(cmd, "ota") == 0) {
        uart_send_str("Rebooting into OTA bootloader...\r\n");
        ble_start_ota_mode();
    } else if (strcmp(cmd, "reboot") == 0) {
        uart_send_str("Rebooting MCU...\r\n");
        WaitMs(50);
        hal_system_soft_reset();
    } else {
        snprintf(resp, sizeof(resp), "Unknown command: '%s'. Type 'help'.\r\n", cmd);
        uart_send_str(resp);
    }

    uart_send_str("cmd>> ");
}
