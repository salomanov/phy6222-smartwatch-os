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

    LOG("Entering Active OSAL Loop...\n");
    uint32_t tick_counter = 0;

    while (1) {
        // Run BLE OSAL scheduler
        osal_run_system();

        // Brief delay (~1ms at 16MHz)
        for (volatile uint32_t i = 0; i < 1500; i++) {
            __asm__ volatile ("nop");
        }

        // Advance OSAL software timers by 1 ms
        osalTimerUpdate(1);

        // Update Watchface & Button every ~20ms
        tick_counter++;
        if (tick_counter >= 20) {
            tick_counter = 0;
            app_os_update();
        }
    }
}

