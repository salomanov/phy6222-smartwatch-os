#ifndef OTA_INAPP_H
#define OTA_INAPP_H

#include <stdint.h>
#include <stdbool.h>

void ota_inapp_init(void);
bool ota_inapp_is_active(void);
uint8_t ota_inapp_get_progress(void);

#endif // OTA_INAPP_H
