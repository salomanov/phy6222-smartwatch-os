#ifndef BATTERY_H
#define BATTERY_H

#include <stdint.h>

void battery_init(void);
uint16_t battery_read_mv(void);
uint8_t battery_get_percent(void);

#endif // BATTERY_H
