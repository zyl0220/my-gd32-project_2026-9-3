/**
 * @file app.c
 * @brief 产品应用入口与非阻塞任务调度。
 *
 * 阅读主线：app_init() 初始化 -> app_tick_1ms_isr() 记录节拍 ->
 * app_process() 在主循环消耗节拍 -> app_process_1ms() 分频执行任务。
 * 当前业务只有 LED 心跳和按键切换 LED；梳子模式记忆尚未接入。
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

/* ISR 写、主循环读，volatile 保证每次读取内存；它不等同于互斥锁。
 * 这里只统计实际执行过的 SysTick ISR，长时间关中断会丢失时间精度。 */
static volatile uint32_t g_app_tick_ms;
/* 主循环已消费到的节拍；其余 elapsed 变量仅在主循环中访问。 */
static uint32_t g_processed_tick_ms;
static uint32_t g_heartbeat_elapsed_ms;
static uint32_t g_key_scan_elapsed_ms;
#if (PROJECT_ENABLE_FACTORY_SELF_TEST != 0U)
static uint32_t g_diagnostic_elapsed_ms;
static uint32_t g_led_mask;
#endif

/* 名称中的 1ms 指一个软件节拍，本函数运行在主循环，不在中断中。
 * 新增周期任务可在此累计时间；耗时操作会延迟同一主循环里的其他任务。 */
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

    /* 正常每 5ms 扫描一次，配合 BSP 的连续 4 次确认形成约 20ms 消抖。
     * DOWN 是一次按下事件，长按不会每次扫描都翻转 LED。 */
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
    /* 有驱动源文件不代表外设已启用：这里只初始化实际使用的 LED 和 WAKEUP。
     * USER 键、PWM、ADC、通用串口、看门狗等需由后续业务显式初始化。 */
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
    /* 例程会写保留区；启动日志先初始化串口，例程才能 printf 输出结果。 */
    flash_eeprom_demo_run();
#endif

    /* 丢弃初始化/自检期间积累的节拍，从初始化结束时开始调度。 */
    g_processed_tick_ms = g_app_tick_ms;
}

void app_process(void)
{
    /* 先取快照，使本轮工作量有限；本轮新增的节拍留给下一轮。
     * 无符号计数自然回绕，使用 != 追赶，避免用 < 判断跨回绕的先后。 */
    uint32_t pending_tick_ms = g_app_tick_ms;

    /* 补跑的是软件任务，不能重建过去的 GPIO 电平。
     * 主循环若长时间阻塞，补跑扫描也不再等价于真实间隔 5ms 的消抖。 */
    while(g_processed_tick_ms != pending_tick_ms) {
        g_processed_tick_ms++;
        app_process_1ms();
    }
}

void app_tick_1ms_isr(void)
{
    /* 中断只记账；按键处理、printf 和 Flash 擦写均留在主循环。 */
    g_app_tick_ms++;
}
