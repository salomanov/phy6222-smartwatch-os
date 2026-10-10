#include "OSAL.h"
#include "OSAL_Tasks.h"
#include "ll.h"
#include "hci_tl.h"
#include "l2cap.h"
#include "gap.h"
#include "gapgattserver.h"
#include "gatt.h"
#include "gattservapp.h"
#include "peripheral.h"
#include "ble_manager.h"

// The order in this table must be identical to the task initialization calls below in osalInitTasks.
__ATTR_SECTION_SRAM__ const pTaskEventHandlerFn tasksArr[] =
{
    LL_ProcessEvent,          // Task 0: Link Layer
    HCI_ProcessEvent,         // Task 1: HCI Transport
    L2CAP_ProcessEvent,       // Task 2: L2CAP Layer
    GAP_ProcessEvent,         // Task 3: GAP Layer
    GATT_ProcessEvent,        // Task 4: GATT Layer
    GAPRole_ProcessEvent,     // Task 5: GAP Peripheral Role
    GATTServApp_ProcessEvent, // Task 6: GATT Service Application
    WatchBLE_ProcessEvent,    // Task 7: Smartwatch App & UI Event Loop
};

__ATTR_SECTION_SRAM__ const uint8 tasksCnt = sizeof(tasksArr) / sizeof(tasksArr[0]);
uint16 *tasksEvents;

void osalInitTasks(void)
{
    uint8 taskID = 0;

    tasksEvents = (uint16 *)osal_mem_alloc(sizeof(uint16) * tasksCnt);
    osal_memset(tasksEvents, 0, (sizeof(uint16) * tasksCnt));

    /* LL Task */
    LL_Init(taskID++);

    /* HCI Task */
    HCI_Init(taskID++);

    /* L2CAP Task */
    L2CAP_Init(taskID++);

    /* GAP Task */
    GAP_Init(taskID++);

    /* GATT Task */
    GATT_Init(taskID++);

    /* Profiles */
    GAPRole_Init(taskID++);
    GATTServApp_Init(taskID++);

    /* Smartwatch App & OTA Service */
    WatchBLE_Init(taskID++);
}
