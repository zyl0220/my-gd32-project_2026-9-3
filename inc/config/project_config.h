/**
 * @file project_config.h
 * @brief 产品级配置入口。产品行为参数只在这里集中修改。
 */

#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

/* 产品身份：发布固件前必须同步更新版本号。 */
#define PRODUCT_NAME                         "GD32C231_PRODUCT"
#define PROJECT_MCU_MODEL                    "GD32C231C8T7"
#define PRODUCT_VERSION_MAJOR                1U
#define PRODUCT_VERSION_MINOR                0U
#define PRODUCT_VERSION_PATCH                0U
#define PRODUCT_VERSION_STRING               "1.0.0"

/* 应用节拍参数。SysTick 固定为 1 ms，app.c 累计节拍后执行对应任务。
 * 心跳参数为“翻转间隔”：500ms 翻转一次对应完整亮灭周期 1s。
 * 按键扫描周期与 bsp_key.h 的消抖次数共同决定按下确认时间。 */
#define APP_HEARTBEAT_HALF_PERIOD_MS         500U
#define APP_KEY_SCAN_PERIOD_MS               5U
#define APP_DIAGNOSTIC_REPORT_PERIOD_MS      1000U

/* 功能开关：只能使用 0U 或 1U。Release(NDEBUG) 默认关闭启动日志。
 * CMakePresets.json 选择构建配置，CMakeLists.txt 将选项转成编译宏。
 * #ifndef 允许编译参数覆盖默认值；#if 为编译期裁剪，并非运行时判断。
 * FactoryTest 启用外接 AT24C02 自检；FlashEepromDemo 启用片内 Flash 计数。
 * 默认 Debug/Release 都不执行这两个例程。 */
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

#ifndef PROJECT_ENABLE_MOTOR_CONTROL
#define PROJECT_ENABLE_MOTOR_CONTROL         0U
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

#if ((PROJECT_ENABLE_MOTOR_CONTROL != 0U) && (PROJECT_ENABLE_MOTOR_CONTROL != 1U))
#error "PROJECT_ENABLE_MOTOR_CONTROL must be 0U or 1U"
#endif

#endif /* PROJECT_CONFIG_H */
