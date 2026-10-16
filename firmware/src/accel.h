#ifndef ACCEL_H
#define ACCEL_H

#include <stdint.h>
#include <stdbool.h>

void accel_init(void);
void accel_update(void);
bool accel_is_detected(void);
void accel_get_xyz(int16_t *x, int16_t *y, int16_t *z);
const char *accel_get_name(void);

#endif // ACCEL_H
