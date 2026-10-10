#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    BTN_EVENT_NONE = 0,
    BTN_EVENT_CLICK,        // Short tap (< 400ms)
    BTN_EVENT_DOUBLE_CLICK, // Quick double tap
    BTN_EVENT_LONG_PRESS,   // Hold > 800ms (Panic trigger!)
    BTN_EVENT_RELEASE,      // Button released
} button_event_t;

void button_init(void);
button_event_t button_update(uint32_t current_ms);
bool button_is_down(void);

#endif // BUTTON_H
