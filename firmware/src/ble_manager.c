#include "ble_manager.h"
#include "bcomdef.h"
#include "OSAL.h"
#include "OSAL_Tasks.h"
#include "OSAL_Timers.h"
#include "gap.h"
#include "gapgattserver.h"
#include "gatt.h"
#include "gattservapp.h"
#include "peripheral.h"
#include "ota_app_service.h"
#include "devinfoservice.h"
#include "app_os.h"
#include "vibrator.h"
#include "log.h"
#include <string.h>

static uint8_t s_task_id = 0;
static bool s_ble_connected = false;

// Device Name in Bluetooth scan: "CHASY-STEALTH"
static uint8_t s_device_name[] = "CHASY-STEALTH";

// GAP - Advertisement data (max 31 bytes)
static uint8_t s_adv_data[] = {
    // Flags
    0x02,
    GAP_ADTYPE_FLAGS,
    GAP_ADTYPE_FLAGS_GENERAL | GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED,

    // Complete Local Name: "CHASY-STEALTH" (13 chars)
    14,
    GAP_ADTYPE_LOCAL_NAME_COMPLETE,
    'C', 'H', 'A', 'S', 'Y', '-', 'S', 'T', 'E', 'A', 'L', 'T', 'H',
};

// GAP - Scan Response data (max 31 bytes)
static uint8_t s_scan_rsp_data[] = {
    // Complete Local Name
    14,
    GAP_ADTYPE_LOCAL_NAME_COMPLETE,
    'C', 'H', 'A', 'S', 'Y', '-', 'S', 'T', 'E', 'A', 'L', 'T', 'H',

    // Connection interval range (100ms - 1000ms)
    0x05,
    GAP_ADTYPE_SLAVE_CONN_INTERVAL_RANGE,
    LO_UINT16(80),
    HI_UINT16(80),
    LO_UINT16(800),
    HI_UINT16(800),

    // Tx power level: 0 dBm
    0x02,
    GAP_ADTYPE_POWER_LEVEL,
    0
};

static void watchBLE_StateNotificationCB(gaprole_States_t newState) {
    switch (newState) {
        case GAPROLE_STARTED:
            LOG("BLE Started\n");
            break;

        case GAPROLE_ADVERTISING:
            LOG("BLE Advertising...\n");
            s_ble_connected = false;
            break;

        case GAPROLE_CONNECTED:
            LOG("BLE CONNECTED!\n");
            s_ble_connected = true;
            vibrator_pulse(40); // Soft buzz upon connection
            break;

        case GAPROLE_WAITING:
        case GAPROLE_WAITING_AFTER_TIMEOUT:
            LOG("BLE Disconnected\n");
            s_ble_connected = false;
            break;

        case GAPROLE_ERROR:
            LOG("BLE Error\n");
            s_ble_connected = false;
            break;

        default:
            break;
    }
}

static gapRolesCBs_t s_peripheral_callbacks = {
    watchBLE_StateNotificationCB,
    NULL
};

bool ble_is_connected(void) {
    return s_ble_connected;
}

void WatchBLE_Init(uint8_t task_id) {
    s_task_id = task_id;

    LOG("WatchBLE_Init task=%d\n", task_id);

    // 1. Setup GAP Peripheral Role Parameters
    uint8_t initial_advertising_enable = TRUE;
    uint16_t adv_int_min = 160; // 100 ms (160 * 0.625ms)
    uint16_t adv_int_max = 160;

    GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t), &initial_advertising_enable);
    GAPRole_SetParameter(GAPROLE_ADVERT_DATA, sizeof(s_adv_data), s_adv_data);
    GAPRole_SetParameter(GAPROLE_SCAN_RSP_DATA, sizeof(s_scan_rsp_data), s_scan_rsp_data);
    GAP_SetParamValue(TGAP_GEN_DISC_ADV_INT_MIN, adv_int_min);
    GAP_SetParamValue(TGAP_GEN_DISC_ADV_INT_MAX, adv_int_max);

    // 2. Setup GAP Service (Device Name)
    GGS_SetParameter(GGS_DEVICE_NAME_ATT, sizeof(s_device_name) - 1, s_device_name);

    // 3. Register Standard GATT Services
    GGS_AddService(GATT_ALL_SERVICES);
    DevInfo_AddService();

    // 4. Register Official PhyPlus BLE OTA Service
    ota_app_AddService();

    // 5. Initialize Display, Button, and Watch OS
    app_os_init();

    // 6. Start Peripheral Role
    GAPRole_StartDevice(&s_peripheral_callbacks);

    // 7. Start periodic 20ms OSAL timer for UI and touch button
    osal_start_reload_timer(s_task_id, WATCH_TICK_EVT, 20);
}

uint16_t WatchBLE_ProcessEvent(uint8_t task_id, uint16_t events) {
    (void)task_id;

    if (events & SYS_EVENT_MSG) {
        uint8_t *pMsg;
        while ((pMsg = osal_msg_receive(s_task_id)) != NULL) {
            osal_msg_deallocate(pMsg);
        }
        return (events ^ SYS_EVENT_MSG);
    }

    if (events & WATCH_TICK_EVT) {
        // Run regular watch update (buttons, display, clock)
        app_os_update();
        return (events ^ WATCH_TICK_EVT);
    }

    return 0;
}
