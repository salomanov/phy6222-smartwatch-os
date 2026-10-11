#include "uart_cli.h"
#include "uart.h"
#include "gpio.h"
#include "log.h"
#include "mcu.h"
#include "clock.h"
#include "app_os.h"
#include "ble_manager.h"
#include "vibrator.h"
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
    uart_send_str("\r\n=== CHASY-STEALTH UART CLI READY ===\r\nType 'help' for commands.\r\ncmd>> ");
}

void uart_cli_update(void) {
    if (!s_cmd_ready) return;

    char cmd[CLI_RX_BUF_SIZE];
    strcpy(cmd, s_rx_buf);
    s_rx_len = 0;
    s_cmd_ready = false;

    // Process command
    if (strcmp(cmd, "ping") == 0) {
        uart_send_str("pong\r\n");
    } else if (strcmp(cmd, "help") == 0) {
        uart_send_str("\r\nCommands:\r\n"
                      "  ping          - Test communication\r\n"
                      "  status        - Show BLE and system status\r\n"
                      "  time HH:MM:SS - Set watch time\r\n"
                      "  vib           - Trigger vibration\r\n"
                      "  ota           - Reboot into OTA bootloader\r\n"
                      "  reboot        - Soft reset MCU\r\n\r\n");
    } else if (strcmp(cmd, "status") == 0) {
        char resp[128];
        uint8_t bst = ble_get_state();
        snprintf(resp, sizeof(resp), "BLE State: %d, Connected: %d, Calls: %d\r\n",
                 bst, ble_is_connected(), ble_get_start_calls());
        uart_send_str(resp);
    } else if (strncmp(cmd, "time ", 5) == 0) {
        int h = 0, m = 0, s = 0;
        if (sscanf(cmd + 5, "%d:%d:%d", &h, &m, &s) >= 2) {
            app_os_set_time((uint8_t)h, (uint8_t)m, (uint8_t)s);
            char resp[64];
            snprintf(resp, sizeof(resp), "Time set to %02d:%02d:%02d\r\n", h, m, s);
            uart_send_str(resp);
        } else {
            uart_send_str("Invalid format. Use: time HH:MM:SS\r\n");
        }
    } else if (strcmp(cmd, "vib") == 0) {
        vibrator_pulse(80);
        uart_send_str("Vibrating...\r\n");
    } else if (strcmp(cmd, "ota") == 0) {
        uart_send_str("Rebooting into OTA bootloader...\r\n");
        ble_start_ota_mode();
    } else if (strcmp(cmd, "reboot") == 0) {
        uart_send_str("Rebooting MCU...\r\n");
        WaitMs(50);
        hal_system_soft_reset();
    } else {
        uart_send_str("Unknown command. Type 'help'.\r\n");
    }

    uart_send_str("cmd>> ");
}
