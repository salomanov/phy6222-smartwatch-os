#ifndef APP_OS_H
#define APP_OS_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    MODE_WATCHFACE = 0,
    MODE_CHEATSHEET,
    MODE_STOPWATCH,
    MODE_OTA_UPDATE,
    MODE_FLASHLIGHT,
    MODE_MAX
} os_mode_t;

void app_os_init(void);
void app_os_update(void);
void app_os_panic(void);

#endif // APP_OS_H
