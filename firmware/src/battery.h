#ifndef BATTERY_H
#define BATTERY_H

#include <stdint.h>
#include <stdbool.h>

void battery_init(void);
uint16_t battery_read_mv(void);
uint8_t battery_get_percent(void);
bool battery_is_charging(void);

#endif // BATTERY_H
