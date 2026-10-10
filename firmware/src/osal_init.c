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

extern pwrmgr_attribute_t pwrmgr_attribute;
extern uint32 ll_remain_time;

#ifdef __GNUC__
void app_osal_init(void) __attribute__((naked));
#endif
void app_osal_init(void)
{
    osal_init_system();
    osal_pwrmgr_device(PWRMGR_BATTERY);
    osal_start_system();
    return;
}
