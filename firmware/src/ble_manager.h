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
uint8_t ble_get_cb_state(void);
uint8_t ble_get_start_ret(void);
uint16_t ble_get_start_calls(void);
uint8_t ble_get_task_id(void);
void ble_try_start_device(void);
void ble_start_ota_mode(void);
void ble_notify_accel(const char *name, int16_t x, int16_t y, int16_t z);

#endif // BLE_MANAGER_H
