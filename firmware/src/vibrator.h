#ifndef VIBRATOR_H
#define VIBRATOR_H

#include <stdint.h>

void vibrator_init(void);
void vibrator_pulse(uint16_t duration_ms);
void vibrator_double_pulse(void);
void vibrator_long_buzz(void);

#endif // VIBRATOR_H
