/**
 * @file startup_log.c
 * @brief 隔离 printf、EVAL 串口和芯片时钟查询等诊断细节。
 */

#include "diagnostics/startup_log.h"

#include <stdio.h>
#include "bsp/bsp_debug_uart.h"
#include "config/project_config.h"
#include "gd32c2x1.h"

void startup_log_print(void)
{
#ifdef __FIRMWARE_VERSION_DEFINE
    uint32_t fw_ver = gd32c2x1_firmware_version_get();
#endif

    /* 日志使用项目 BSP 的 USART0（PB6/PB7），printf 重定向见 gd32c231c_eval.c。
     * 通用 bsp_uart 使用另一套 USART1；调用它不会自动切换 printf 的端口。 */
    bsp_debug_uart_init();
    printf("\r\n[BOOT] %s V%s", PRODUCT_NAME, PRODUCT_VERSION_STRING);
#ifdef __FIRMWARE_VERSION_DEFINE
    printf("\r\n[BOOT] GD32C2X1 firmware V%u.%u.%u",
           (unsigned int)(uint8_t)(fw_ver >> 24),
           (unsigned int)(uint8_t)(fw_ver >> 16),
           (unsigned int)(uint8_t)(fw_ver >> 8));
#endif
    printf("\r\n[BOOT] CK_SYS=%lu CK_AHB=%lu CK_APB=%lu\r\n",
           (unsigned long)rcu_clock_freq_get(CK_SYS),
           (unsigned long)rcu_clock_freq_get(CK_AHB),
           (unsigned long)rcu_clock_freq_get(CK_APB));
}
