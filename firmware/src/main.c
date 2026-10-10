#include "config.h"
#include "gpio.h"
#include "clock.h"
#include "global_config.h"
#include "jump_function.h"
#include "pwrmgr.h"
#include "mcu.h"
#include "log.h"
#include "rf_phy_driver.h"
#include "flash.h"
#include "app_os.h"
#include "ll.h"
#include "ll_def.h"

extern void init_config(void);
extern void hal_rom_boot_init(void);

// BLE LL Connection Context Buffers
#define BLE_MAX_ALLOW_CONNECTION       1
#define BLE_MAX_ALLOW_PKT_PER_EVENT_TX 2
#define BLE_MAX_ALLOW_PKT_PER_EVENT_RX 2
#define BLE_PKT_VERSION                BLE_PKT_VERSION_5_1

#define BLE_PKT_BUF_SIZE (((BLE_PKT_VERSION == BLE_PKT_VERSION_5_1) ? 1 : 0) * BLE_PKT51_LEN \
                        + ((BLE_PKT_VERSION == BLE_PKT_VERSION_4_0) ? 1 : 0) * BLE_PKT40_LEN \
                        + (sizeof(struct ll_pkt_desc) - 2))

#define BLE_MAX_ALLOW_PER_CONNECTION (BLE_PKT_BUF_SIZE)
#define BLE_CONN_BUF_SIZE            (BLE_MAX_ALLOW_CONNECTION * BLE_MAX_ALLOW_PER_CONNECTION)

ALIGN4_U8 g_pConnectionBuffer[BLE_CONN_BUF_SIZE];
llConnState_t pConnContext[BLE_MAX_ALLOW_CONNECTION];

// OSAL Heap & LL Link Heap Buffers
#define LARGE_HEAP_SIZE (4 * 1024)
ALIGN4_U8 g_largeHeap[LARGE_HEAP_SIZE];

#define LL_LINKBUF_CFG_NUM 0
#define LL_PKT_BUFSIZE     280
#define LL_LINK_HEAP_SIZE  ((BLE_MAX_ALLOW_CONNECTION * 3 + LL_LINKBUF_CFG_NUM) * LL_PKT_BUFSIZE)
ALIGN4_U8 g_llLinkHeap[LL_LINK_HEAP_SIZE];

volatile uint8 g_clk32K_config;
volatile sysclk_t g_spif_clk_config;

static void hal_low_power_io_init(void) {
    const ioinit_cfg_t ioInit[] = {
        {GPIO_LED,      GPIO_PULL_DOWN},
        {BKL_PIN,       GPIO_PULL_UP},   // Backlight off default (high)
        {GPIO_VIBRATOR, GPIO_PULL_DOWN},
        {UART_TX_PIN,   GPIO_PULL_UP},   // TX
        {UART_RX_PIN,   GPIO_PULL_UP},   // RX
        {BUTTON_PIN,    GPIO_PULL_DOWN}, // Touch Key
        {RST_PIN,       GPIO_FLOATING},  // Display RST
        {DC_PIN,        GPIO_FLOATING},  // Display DC
        {CS_PIN,        GPIO_FLOATING},  // Display CS
        {MOSI_PIN,      GPIO_FLOATING},  // Display SDA
        {SCLK_PIN,      GPIO_FLOATING},  // Display SCL
    };

    for (uint8_t i = 0; i < sizeof(ioInit) / sizeof(ioinit_cfg_t); i++) {
        hal_gpio_pull_set(ioInit[i].pin, ioInit[i].type);
    }

    DCDC_CONFIG_SETTING(0x0a);
    DCDC_REF_CLK_SETTING(1);
    DIG_LDO_CURRENT_SETTING(1);
    hal_pwrmgr_RAM_retention(RET_SRAM0 | RET_SRAM1 | RET_SRAM2);
    hal_pwrmgr_RAM_retention_set();
    hal_pwrmgr_LowCurrentLdo_enable();
    subWriteReg(0x4000f014, 26, 26, 1);
}



static void ble_mem_init_config(void) {
    extern void ll_osalmem_init(osalMemHdr_t *hdr, uint32 size);
    ll_osalmem_init((osalMemHdr_t *)g_llLinkHeap, LL_LINK_HEAP_SIZE);
    osal_mem_set_heap((osalMemHdr_t *)g_largeHeap, LARGE_HEAP_SIZE);
    LL_InitConnectContext(pConnContext,
                          g_pConnectionBuffer,
                          BLE_MAX_ALLOW_CONNECTION,
                          BLE_MAX_ALLOW_PKT_PER_EVENT_TX,
                          BLE_MAX_ALLOW_PKT_PER_EVENT_RX,
                          BLE_PKT_VERSION);
}

static void hal_rfphy_init(void) {
    g_rfPhyTxPower = RF_PHY_TX_POWER_0DBM;
    g_rfPhyPktFmt = PKT_FMT_BLE1M;
    g_rfPhyFreqOffSet = RF_PHY_FREQ_FOFF_00KHZ;
    XTAL16M_CAP_SETTING(0x09);
    XTAL16M_CURRENT_SETTING(0x03);

    hal_rc32k_clk_tracking_init();
    hal_rom_boot_init();

    NVIC_SetPriority((IRQn_Type)BB_IRQn,   IRQ_PRIO_REALTIME);
    NVIC_SetPriority((IRQn_Type)TIM1_IRQn, IRQ_PRIO_HIGH); // ll_EVT
    NVIC_SetPriority((IRQn_Type)TIM2_IRQn, IRQ_PRIO_HIGH); // OSAL_TICK
    NVIC_SetPriority((IRQn_Type)TIM4_IRQn, IRQ_PRIO_HIGH); // LL_EXA_ADV

    NVIC_EnableIRQ((IRQn_Type)BB_IRQn);
    NVIC_EnableIRQ((IRQn_Type)TIM1_IRQn);
    NVIC_EnableIRQ((IRQn_Type)TIM2_IRQn);
    NVIC_EnableIRQ((IRQn_Type)TIM4_IRQn);

    ble_mem_init_config();

    extern void rf_phy_ini(void);
    rf_phy_ini();

    extern void rf_calibrate1(void);
    rf_calibrate1();

    extern void hal_rfPhyFreqOff_Set(void);
    hal_rfPhyFreqOff_Set();
}

static void hal_init(void) {
    hal_low_power_io_init();
    clk_init(g_system_clk);
    hal_rtc_clock_config((CLK32K_e)g_clk32K_config);
    hal_pwrmgr_init();
    hal_spif_cache_init(SYS_CLK_DLL_64M, XFRD_FCMD_READ_DUAL);
    hal_gpio_init();
    LOG_INIT();
}

int main(void) {
    g_system_clk = SYS_CLK_XTAL_16M;
    g_clk32K_config = CLK_32K_RCOSC;

    #if defined ( __GNUC__ )
    extern void* osal_memcpy(void* dest, const void* src, unsigned int len);
    osal_memcpy((void*)0x1fff0000, (void*)jump_table_base, 1024);
    #endif

    drv_irq_init();
    init_config();

    extern void ll_patch_slave(void);
    ll_patch_slave();

    hal_rfphy_init();
    hal_init();

    LOG("PHY6222 LP715 Custom OS + BLE Starting...\n");

    // Initialize display and UI immediately so screen shows watchface right away
    app_os_init();

    // Start OSAL and launch app
    extern void app_osal_init(void);
    app_osal_init();

    return 0;
}
