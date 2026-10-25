#ifndef APP_OS_H
#define APP_OS_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    MODE_WATCHFACE = 0,
    MODE_VIBRATOR_TEST,
    MODE_OTA_UPDATE,
    MODE_NOTIFICATION,
    MODE_MAX
} os_mode_t;

void app_os_init(void);
void app_os_update(void);
void app_os_panic(void);
void app_os_set_time(uint8_t h, uint8_t m, uint8_t s);
void app_os_force_redraw(void);
void app_os_show_message(const char *text, uint16_t color, uint8_t scale);
void app_os_dismiss_message(void);

#endif // APP_OS_H
