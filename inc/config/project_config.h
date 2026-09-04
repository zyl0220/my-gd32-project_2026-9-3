/**
 * @file project_config.h
 * @brief 产品级配置入口。产品行为参数只在这里集中修改。
 */

#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

/* 产品身份：发布固件前必须同步更新版本号。 */
#define PRODUCT_NAME                         "GD32C231_PRODUCT"
#define PRODUCT_VERSION_MAJOR                1U
#define PRODUCT_VERSION_MINOR                0U
#define PRODUCT_VERSION_PATCH                0U
#define PRODUCT_VERSION_STRING               "1.0.0"

/* 应用节拍参数。SysTick 固定为 1 ms。 */
#define APP_HEARTBEAT_HALF_PERIOD_MS         500U
#define APP_KEY_SCAN_PERIOD_MS               5U
#define APP_DIAGNOSTIC_REPORT_PERIOD_MS      1000U

/* 功能开关：只能使用 0U 或 1U。Release(NDEBUG) 默认关闭启动日志。 */
#ifndef PROJECT_ENABLE_STARTUP_LOG
#ifdef NDEBUG
#define PROJECT_ENABLE_STARTUP_LOG           0U
#else
#define PROJECT_ENABLE_STARTUP_LOG           1U
#endif
#endif

#ifndef PROJECT_ENABLE_FACTORY_SELF_TEST
#define PROJECT_ENABLE_FACTORY_SELF_TEST     0U
#endif

#ifndef PROJECT_ENABLE_FLASH_EEPROM_DEMO
#define PROJECT_ENABLE_FLASH_EEPROM_DEMO      0U
#endif

#if (APP_HEARTBEAT_HALF_PERIOD_MS == 0U)
#error "APP_HEARTBEAT_HALF_PERIOD_MS must be greater than zero"
#endif

#if (APP_KEY_SCAN_PERIOD_MS == 0U)
#error "APP_KEY_SCAN_PERIOD_MS must be greater than zero"
#endif

#if (APP_DIAGNOSTIC_REPORT_PERIOD_MS == 0U)
#error "APP_DIAGNOSTIC_REPORT_PERIOD_MS must be greater than zero"
#endif

#if ((PROJECT_ENABLE_STARTUP_LOG != 0U) && (PROJECT_ENABLE_STARTUP_LOG != 1U))
#error "PROJECT_ENABLE_STARTUP_LOG must be 0U or 1U"
#endif

#if ((PROJECT_ENABLE_FACTORY_SELF_TEST != 0U) && (PROJECT_ENABLE_FACTORY_SELF_TEST != 1U))
#error "PROJECT_ENABLE_FACTORY_SELF_TEST must be 0U or 1U"
#endif

#if ((PROJECT_ENABLE_FLASH_EEPROM_DEMO != 0U) && (PROJECT_ENABLE_FLASH_EEPROM_DEMO != 1U))
#error "PROJECT_ENABLE_FLASH_EEPROM_DEMO must be 0U or 1U"
#endif

#if ((PROJECT_ENABLE_FLASH_EEPROM_DEMO != 0U) && (PROJECT_ENABLE_STARTUP_LOG == 0U))
#error "Flash EEPROM demo requires PROJECT_ENABLE_STARTUP_LOG"
#endif

#endif /* PROJECT_CONFIG_H */
