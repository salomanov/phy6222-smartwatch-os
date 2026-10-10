#include "OSAL.h"
#include "OSAL_Tasks.h"
#include "OSAL_PwrMgr.h"
#include "osal_snv.h"
#include "ll_sleep.h"
#include "uart.h"
#include "comdef.h"
#include "OSAL_Timers.h"
#include "timer.h"
#include "jump_function.h"
#include "global_config.h"
#include "log.h"
#include "app_os.h"

void app_osal_init(void)
{
    LOG("Initializing OSAL...\n");
    osal_init_system();
    osal_pwrmgr_device(PWRMGR_ALWAYS_ON);

    LOG("Starting OSAL System Loop...\n");
    osal_start_system(); // DOES NOT RETURN
}

