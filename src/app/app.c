/**
 * @file app.c
 * @brief 产品应用入口与非阻塞任务调度。
 */

#include "app/app.h"
#include "config/project_config.h"
#include "bsp/bsp_key.h"
#include "bsp/bsp_led.h"

#if (PROJECT_ENABLE_STARTUP_LOG != 0U)
#include "diagnostics/startup_log.h"
#endif

#if (PROJECT_ENABLE_FACTORY_SELF_TEST != 0U)
#include "bsp/bsp_i2c_eeprom.h"
#include "diagnostics/auto_probe.h"
#endif

#if (PROJECT_ENABLE_FLASH_EEPROM_DEMO != 0U)
#include "examples/flash_eeprom_demo.h"
#endif

static volatile uint32_t g_app_tick_ms;
static uint32_t g_processed_tick_ms;
static uint32_t g_heartbeat_elapsed_ms;
static uint32_t g_key_scan_elapsed_ms;
#if (PROJECT_ENABLE_FACTORY_SELF_TEST != 0U)
static uint32_t g_diagnostic_elapsed_ms;
static uint32_t g_led_mask;
#endif

static void app_process_1ms(void)
{
    g_heartbeat_elapsed_ms++;
    if(g_heartbeat_elapsed_ms >= APP_HEARTBEAT_HALF_PERIOD_MS) {
        g_heartbeat_elapsed_ms = 0U;
        bsp_led_toggle(BSP_LED1);
#if (PROJECT_ENABLE_FACTORY_SELF_TEST != 0U)
        g_led_mask ^= (1UL << (uint32_t)BSP_LED1);
#endif
    }

    g_key_scan_elapsed_ms++;
    if(g_key_scan_elapsed_ms >= APP_KEY_SCAN_PERIOD_MS) {
        g_key_scan_elapsed_ms = 0U;
        if(BSP_KEY_EVENT_DOWN == bsp_key_scan(BSP_KEY_WAKEUP)) {
            bsp_led_toggle(BSP_LED2);
#if (PROJECT_ENABLE_FACTORY_SELF_TEST != 0U)
            g_led_mask ^= (1UL << (uint32_t)BSP_LED2);
#endif
        }
    }

#if (PROJECT_ENABLE_FACTORY_SELF_TEST != 0U)
    g_diagnostic_elapsed_ms++;
    if(g_diagnostic_elapsed_ms >= APP_DIAGNOSTIC_REPORT_PERIOD_MS) {
        g_diagnostic_elapsed_ms = 0U;
        auto_probe_tick(g_led_mask);
    }
#endif
}

void app_init(void)
{
    bsp_led_init(BSP_LED1);
    bsp_led_init(BSP_LED2);
    bsp_key_init(BSP_KEY_WAKEUP);

#if (PROJECT_ENABLE_FACTORY_SELF_TEST != 0U)
    bsp_i2c_eeprom_init();
    auto_probe_init();
#endif

#if (PROJECT_ENABLE_STARTUP_LOG != 0U)
    startup_log_print();
#endif

#if (PROJECT_ENABLE_FLASH_EEPROM_DEMO != 0U)
    flash_eeprom_demo_run();
#endif

    g_processed_tick_ms = g_app_tick_ms;
}

void app_process(void)
{
    uint32_t pending_tick_ms = g_app_tick_ms;

    while(g_processed_tick_ms != pending_tick_ms) {
        g_processed_tick_ms++;
        app_process_1ms();
    }
}

void app_tick_1ms_isr(void)
{
    g_app_tick_ms++;
}
