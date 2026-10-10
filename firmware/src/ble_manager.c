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
#include "att.h"
#include "ll_common.h"
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

static volatile uint8_t s_ble_state = 0;
static volatile uint8_t s_start_ret = 0xFF;
static volatile uint16_t s_start_calls = 0;

static void watchBLE_StateNotificationCB(gaprole_States_t newState) {
    s_ble_state = (uint8_t)newState;
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
            break;

        case GAPROLE_WAITING:
        case GAPROLE_WAITING_AFTER_TIMEOUT:
            LOG("BLE Disconnected or Adv Timeout -> Restarting Advertising\n");
            s_ble_connected = false;
            // Instantly re-enable advertising so it never stops!
            {
                uint8_t advert_enable = TRUE;
                GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t), &advert_enable);
            }
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

uint8_t ble_get_state(void) {
    uint8_t state = 0;
    if (GAPRole_GetParameter(GAPROLE_STATE, &state) == SUCCESS) {
        return state;
    }
    return s_ble_state;
}

uint8_t ble_get_cb_state(void) {
    return s_ble_state;
}

uint8_t ble_get_start_ret(void) {
    return s_start_ret;
}

uint16_t ble_get_start_calls(void) {
    return s_start_calls;
}

uint8_t ble_get_task_id(void) {
    return s_task_id;
}

void ble_try_start_device(void) {
    s_start_calls++;
    s_start_ret = GAPRole_StartDevice(&s_peripheral_callbacks);
    LOG("GAPRole_StartDevice() ret=0x%02X calls=%d\n", s_start_ret, s_start_calls);
}

void WatchBLE_Init(uint8_t task_id) {
    s_task_id = task_id;

    LOG("WatchBLE_Init task=%d\n", task_id);

    // GAP peripheral pause (SDK default is 6 seconds before parameter update)
    GAP_SetParamValue(TGAP_CONN_PAUSE_PERIPHERAL, 6);

    // 1. Setup GAP Peripheral Role Parameters
    uint8_t initial_advertising_enable = TRUE;
    uint8_t enable_update_request = TRUE;
    uint8_t adv_chn_map = GAP_ADVCHAN_37 | GAP_ADVCHAN_38 | GAP_ADVCHAN_39;
    uint8_t adv_type = GAP_ADTYPE_ADV_IND; // Connectable undirected advertisement
    uint16_t gap_role_advert_off_time = 0;
    uint16_t desired_min_interval = 80;   // 100ms (SDK standard)
    uint16_t desired_max_interval = 800;  // 1000ms (SDK standard)
    uint16_t desired_slave_latency = 0;
    uint16_t desired_conn_timeout = 1000; // 10s (SDK standard)

    GAPRole_SetParameter(GAPROLE_ADV_EVENT_TYPE, sizeof(uint8_t), &adv_type);
    GAPRole_SetParameter(GAPROLE_ADV_CHANNEL_MAP, sizeof(uint8_t), &adv_chn_map);
    GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t), &initial_advertising_enable);
    GAPRole_SetParameter(GAPROLE_ADVERT_OFF_TIME, sizeof(uint16_t), &gap_role_advert_off_time);
    GAPRole_SetParameter(GAPROLE_ADVERT_DATA, sizeof(s_adv_data), s_adv_data);
    GAPRole_SetParameter(GAPROLE_SCAN_RSP_DATA, sizeof(s_scan_rsp_data), s_scan_rsp_data);
    GAPRole_SetParameter(GAPROLE_PARAM_UPDATE_ENABLE, sizeof(uint8_t), &enable_update_request);
    GAPRole_SetParameter(GAPROLE_MIN_CONN_INTERVAL, sizeof(uint16_t), &desired_min_interval);
    GAPRole_SetParameter(GAPROLE_MAX_CONN_INTERVAL, sizeof(uint16_t), &desired_max_interval);
    GAPRole_SetParameter(GAPROLE_SLAVE_LATENCY, sizeof(uint16_t), &desired_slave_latency);
    GAPRole_SetParameter(GAPROLE_TIMEOUT_MULTIPLIER, sizeof(uint16_t), &desired_conn_timeout);

    // Set advertising interval: 160 * 625us = 100ms
    uint16_t adv_int = 160;
    GAP_SetParamValue(TGAP_GEN_DISC_ADV_INT_MIN, adv_int);
    GAP_SetParamValue(TGAP_GEN_DISC_ADV_INT_MAX, adv_int);
    GAP_SetParamValue(TGAP_LIM_DISC_ADV_INT_MIN, adv_int);
    GAP_SetParamValue(TGAP_LIM_DISC_ADV_INT_MAX, adv_int);

    // 2. Setup GAP Service (Device Name)
    GGS_SetParameter(GGS_DEVICE_NAME_ATT, sizeof(s_device_name) - 1, s_device_name);

    // 3. Register Standard GATT Services
    GGS_AddService(GATT_ALL_SERVICES);
    GATTServApp_AddService(GATT_ALL_SERVICES);
    // DevInfo_AddService(); // Omitted in SDK examples to avoid GATT attribute conflicts

    // 4. Register Official PhyPlus BLE OTA Service
    ota_app_AddService();

    // 5. Setup BLE 4.2 / 5.0 High-Speed Features (MTU)
    ATT_SetMTUSizeMax(240);
    llInitFeatureSet2MPHY(TRUE);
    llInitFeatureSetDLE(TRUE);

    // 6. Schedule peripheral device start event
    osal_set_event(task_id, WATCH_START_DEVICE_EVT);

    // 7. Start periodic UI & input update timer (every 50 ms)
    osal_start_reload_timer(task_id, WATCH_TICK_EVT, 50);
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

    if (events & WATCH_START_DEVICE_EVT) {
        LOG("WatchBLE: Starting GAPRole Device Advertising...\n");
        ble_try_start_device();
        return (events ^ WATCH_START_DEVICE_EVT);
    }

    if (events & WATCH_TICK_EVT) {
        // Run regular watch update (buttons, display, clock)
        app_os_update();
        return (events ^ WATCH_TICK_EVT);
    }

    return 0;
}
