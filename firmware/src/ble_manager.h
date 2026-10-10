#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#define WATCH_TICK_EVT          0x0001
#define WATCH_STATE_EVT         0x0002
#define WATCH_START_DEVICE_EVT  0x0004

void WatchBLE_Init(uint8_t task_id);
uint16_t WatchBLE_ProcessEvent(uint8_t task_id, uint16_t events);

bool ble_is_connected(void);
uint8_t ble_get_state(void);

#endif // BLE_MANAGER_H
