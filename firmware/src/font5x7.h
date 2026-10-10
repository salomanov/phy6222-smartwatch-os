#ifndef FONT5X7_H
#define FONT5X7_H

#include <stdint.h>

// Standard 5x7 bitmap font (ASCII 32 to 127)
// Each character is 5 bytes wide, 7 bits high (MSB unused or top-to-bottom)
extern const uint8_t font5x7_data[];

// Russian CP1251 uppercase/lowercase support (0xC0 to 0xFF -> 64 chars)
extern const uint8_t font5x7_cyrillic[];

#endif // FONT5X7_H
