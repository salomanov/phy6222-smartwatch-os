#ifndef ACCEL_H
#define ACCEL_H

#include <stdint.h>
#include <stdbool.h>
#include "gpio.h"

void accel_init(void);
void accel_update(void);
bool accel_is_detected(void);
void accel_get_xyz(int16_t *x, int16_t *y, int16_t *z);
const char *accel_get_name(void);
const char *accel_get_pins(void);
uint8_t accel_get_addr(void);
uint8_t accel_get_chip_id(void);

#endif // ACCEL_H
