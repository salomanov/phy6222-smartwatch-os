#include "OSAL.h"
#include "OSAL_Tasks.h"
#include "osal_cbtimer.h"
#include "ll.h"
#include "app_os.h"

const pTaskEventHandlerFn tasksArr[] =
{
    LL_ProcessEvent,
#if defined(OSAL_CBTIMER_NUM_TASKS)
    OSAL_CBTIMER_PROCESS_EVENT(osal_CbTimerProcessEvent),
#endif
};

const uint8 tasksCnt = sizeof(tasksArr) / sizeof(tasksArr[0]);
uint16 *tasksEvents;

void osalInitTasks(void)
{
    uint8 taskID = 0;

    tasksEvents = (uint16 *)osal_mem_alloc(sizeof(uint16) * tasksCnt);
    osal_memset(tasksEvents, 0, (sizeof(uint16) * tasksCnt));

    LL_Init(taskID++);
#if defined(OSAL_CBTIMER_NUM_TASKS)
    osal_CbTimerInit(taskID);
    taskID += OSAL_CBTIMER_NUM_TASKS;
#endif

    // Start our custom smartwatch OS
    app_os_init();

    while (1) {
        app_os_update();
    }
}
