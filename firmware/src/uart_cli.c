#include "uart_cli.h"
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

static void uart_send_str(const char *str) {
    hal_uart_send_buff(UART0, (uint8_t *)str, strlen(str));
}

// Low-level Bit-Bang I2C helper for CLI commands on P16/P17
static void cli_i2c_delay(void) {
    for (volatile int i = 0; i < 40; i++) {
        __asm__ volatile ("nop");
    }
}

static void cli_i2c_sda(bool hi) {
    if (hi) {
        hal_gpio_pin_init(GPIO_P17, GPIO_INPUT);
        hal_gpio_pull_set(GPIO_P17, GPIO_PULL_UP_S);
    } else {
        hal_gpio_pin_init(GPIO_P17, GPIO_OUTPUT);
        hal_gpio_write(GPIO_P17, 0);
    }
}

static bool cli_i2c_sda_in(void) {
    hal_gpio_pin_init(GPIO_P17, GPIO_INPUT);
    hal_gpio_pull_set(GPIO_P17, GPIO_PULL_UP_S);
    return hal_gpio_read(GPIO_P17);
}

static void cli_i2c_scl(bool hi) {
    if (hi) {
        hal_gpio_pin_init(GPIO_P16, GPIO_INPUT);
        hal_gpio_pull_set(GPIO_P16, GPIO_PULL_UP_S);
    } else {
        hal_gpio_pin_init(GPIO_P16, GPIO_OUTPUT);
        hal_gpio_write(GPIO_P16, 0);
    }
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
    cli_i2c_sda(true);
    cli_i2c_delay();
    cli_i2c_scl(true);
    cli_i2c_delay();
    bool ack = !cli_i2c_sda_in();
    cli_i2c_scl(false);
    cli_i2c_delay();
    return ack;
}

static uint8_t cli_i2c_read_byte(bool ack) {
    uint8_t byte = 0;
    cli_i2c_sda(true);
    for (int i = 7; i >= 0; i--) {
        cli_i2c_scl(true);
        cli_i2c_delay();
        if (cli_i2c_sda_in()) byte |= (1 << i);
        cli_i2c_scl(false);
        cli_i2c_delay();
    }
    cli_i2c_sda(!ack);
    cli_i2c_delay();
    cli_i2c_scl(true);
    cli_i2c_delay();
    cli_i2c_scl(false);
    cli_i2c_delay();
    return byte;
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
        uart_send_str("\r\n=== HARDWARE CLI COMMANDS ===\r\n"
                      "  pin <p> <0|1>       - Set GPIO pin level\r\n"
                      "  read <p>            - Read GPIO pin input level\r\n"
                      "  bkl <1|0>           - Backlight ON (1) or OFF (0)\r\n"
                      "  disp                - Re-initialize display\r\n"
                      "  vib [ms]            - Trigger vibrator (default 80ms)\r\n"
                      "  i2c scan            - Scan I2C addresses on P16/P17\r\n"
                      "  i2c r <hex_a> <reg> - Read I2C register (e.g. i2c r 19 0f)\r\n"
                      "  i2c w <a> <r> <val> - Write I2C register\r\n"
                      "  status              - Show BLE, battery, sensor status\r\n"
                      "  time HH:MM:SS       - Set watch clock\r\n"
                      "  ota                 - Reboot into OTA bootloader\r\n"
                      "  reboot              - Soft reset MCU\r\n\r\n");
    } else if (strncmp(cmd, "pin ", 4) == 0) {
        int pin = -1, val = -1;
        if (sscanf(cmd + 4, "%d %d", &pin, &val) == 2 && pin >= 0 && pin <= 34) {
            // Guard against P33 or display pins
            if (pin == 33 || pin == 24 || pin == 25 || pin == 31 || pin == 32 || pin == 34) {
                snprintf(resp, sizeof(resp), "[WARN] Pin P%d is reserved for ST7735 display! Skipped.\r\n", pin);
            } else {
                hal_gpio_cfg_analog_io((gpio_pin_e)pin, Bit_DISABLE);
                hal_gpio_pin_init((gpio_pin_e)pin, GPIO_OUTPUT);
                hal_gpio_write((gpio_pin_e)pin, val ? 1 : 0);
                snprintf(resp, sizeof(resp), "[OK] Pin P%d set to %d\r\n", pin, val ? 1 : 0);
            }
            uart_send_str(resp);
        } else {
            uart_send_str("Usage: pin <pin_num> <0|1>\r\n");
        }
    } else if (strncmp(cmd, "read ", 5) == 0) {
        int pin = -1;
        if (sscanf(cmd + 5, "%d", &pin) == 1 && pin >= 0 && pin <= 34) {
            hal_gpio_cfg_analog_io((gpio_pin_e)pin, Bit_DISABLE);
            hal_gpio_pin_init((gpio_pin_e)pin, GPIO_INPUT);
            hal_gpio_pull_set((gpio_pin_e)pin, GPIO_PULL_UP_S);
            WaitUs(50);
            int level = hal_gpio_read((gpio_pin_e)pin);
            snprintf(resp, sizeof(resp), "[OK] Pin P%d = %d\r\n", pin, level);
            uart_send_str(resp);
        } else {
            uart_send_str("Usage: read <pin_num>\r\n");
        }
    } else if (strncmp(cmd, "bkl ", 4) == 0) {
        int val = -1;
        if (sscanf(cmd + 4, "%d", &val) == 1) {
            if (val) {
                backlight_on();
                uart_send_str("[OK] Backlight ON\r\n");
            } else {
                backlight_off();
                uart_send_str("[OK] Backlight OFF\r\n");
            }
        }
    } else if (strcmp(cmd, "disp") == 0) {
        display_init(0);
        uart_send_str("[OK] Display re-initialized.\r\n");
    } else if (strncmp(cmd, "vib", 3) == 0) {
        int ms = 80;
        sscanf(cmd + 3, "%d", &ms);
        if (ms <= 0 || ms > 2000) ms = 80;
        vibrator_pulse((uint32_t)ms);
        snprintf(resp, sizeof(resp), "[OK] Vibration pulse %d ms\r\n", ms);
        uart_send_str(resp);
    } else if (strcmp(cmd, "i2c scan") == 0) {
        uart_send_str("Scanning I2C bus on P16 (SCL) / P17 (SDA)...\r\n");
        subWriteReg(&(AP_AON->PMCTL2_0), 6, 6, 0x01);
        cli_i2c_scl(true);
        cli_i2c_sda(true);
        WaitMs(5);

        int count = 0;
        for (uint8_t a = 0x08; a <= 0x77; a++) {
            cli_i2c_start();
            bool ack = cli_i2c_write_byte((a << 1) | 0);
            cli_i2c_stop();
            if (ack) {
                count++;
                snprintf(resp, sizeof(resp), "  -> Found ACK at 7-bit addr: 0x%02X (8-bit: 0x%02X)\r\n", a, a << 1);
                uart_send_str(resp);
            }
        }
        snprintf(resp, sizeof(resp), "Scan complete. Found %d devices.\r\n", count);
        uart_send_str(resp);
    } else if (strncmp(cmd, "i2c r ", 6) == 0) {
        unsigned int a = 0, reg = 0;
        if (sscanf(cmd + 6, "%x %x", &a, &reg) == 2) {
            cli_i2c_start();
            if (!cli_i2c_write_byte((a << 1) | 0)) {
                cli_i2c_stop();
                uart_send_str("[ERR] NAK on addr write\r\n");
            } else if (!cli_i2c_write_byte(reg)) {
                cli_i2c_stop();
                uart_send_str("[ERR] NAK on reg write\r\n");
            } else {
                cli_i2c_start();
                if (!cli_i2c_write_byte((a << 1) | 1)) {
                    cli_i2c_stop();
                    uart_send_str("[ERR] NAK on addr read\r\n");
                } else {
                    uint8_t val = cli_i2c_read_byte(false);
                    cli_i2c_stop();
                    snprintf(resp, sizeof(resp), "[OK] Addr 0x%02X Reg 0x%02X = 0x%02X (%d)\r\n", a, reg, val, val);
                    uart_send_str(resp);
                }
            }
        } else {
            uart_send_str("Usage: i2c r <hex_addr> <hex_reg> (e.g. i2c r 19 0f)\r\n");
        }
    } else if (strncmp(cmd, "i2c w ", 6) == 0) {
        unsigned int a = 0, reg = 0, val = 0;
        if (sscanf(cmd + 6, "%x %x %x", &a, &reg, &val) == 3) {
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
        int16_t ax = 0, ay = 0, az = 0;
        accel_get_xyz(&ax, &ay, &az);
        snprintf(resp, sizeof(resp), "BLE: %s, Sensor: %s (X=%d Y=%d Z=%d), Bat: %d%% (%s)\r\n",
                 ble_is_connected() ? "CONN" : "ADV",
                 accel_get_name(), (int)ax, (int)ay, (int)az,
                 battery_get_percent(), battery_is_charging() ? "CHG" : "DISCHG");
        uart_send_str(resp);
    } else if (strncmp(cmd, "time ", 5) == 0) {
        int h = 0, m = 0, s = 0;
        if (sscanf(cmd + 5, "%d:%d:%d", &h, &m, &s) >= 2) {
            app_os_set_time((uint8_t)h, (uint8_t)m, (uint8_t)s);
            snprintf(resp, sizeof(resp), "Time set to %02d:%02d:%02d\r\n", h, m, s);
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
