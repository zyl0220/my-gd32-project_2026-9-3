/*!
    \file    auto_probe.c
    \brief   自主调试闭环 - 双回传通道探针实现

    通道B (UART printf): bsp_debug_uart_init()，USART0 PB6/PB7 115200 8N1 +
                         固件已有的 __io_putchar 重定向，printf 直接出串口。
    通道A (读内存/RTT):  g_test_block 全局块，J-Link 用 .map 里的地址在线读取，
                         无需额外接线，最稳。

    \note 本模块仅供 FactoryTest 构建使用。实际资源占用以对应构建的
          .map 文件和链接器内存报告为准。
*/

#include <stdio.h>
#include "diagnostics/auto_probe.h"
#include "gd32c2x1.h"
#include "bsp/bsp_debug_uart.h"
#include "bsp/bsp_led.h"
#include "bsp/bsp_i2c_eeprom.h"

/* volatile + 在 tick 中被写入 -> 不会被 --gc-sections 回收，地址稳定可读 */
volatile auto_test_block_t g_test_block = {
    AUTO_TEST_MAGIC, 0U, 0U, 0U
};

/*!
    \brief  上电自测：检查基础不变量，结果写入 g_test_block.test_pass
    \retval 1=全过, 0=有失败
*/
static uint32_t auto_self_test(void)
{
    uint32_t pass = 1U;

    /* 自测1: LED 枚举范围合法(BSP_LED_NUM 应为正且不超过工程定义上限) */
    if((BSP_LED_NUM == 0U) || (BSP_LED_NUM > 8U)) {
        pass = 0U;
        printf("[SELFTEST][FAIL] BSP_LED_NUM out of range: %u\r\n", (unsigned)BSP_LED_NUM);
    } else {
        printf("[SELFTEST][PASS] BSP_LED_NUM = %u\r\n", (unsigned)BSP_LED_NUM);
    }

    /* 自测2: 结果块自身 magic 完好(粗略验证 .data 初始化正确) */
    if(g_test_block.magic != AUTO_TEST_MAGIC) {
        pass = 0U;
        printf("[SELFTEST][FAIL] test_block magic corrupt: %08lX\r\n",
               (unsigned long)g_test_block.magic);
    } else {
        printf("[SELFTEST][PASS] test_block magic OK\r\n");
    }

    /* 自测3: I2C EEPROM 写-读-比对 (需 AT24C02 在位; 无器件会超时判 FAIL) */
    if(BSP_EEPROM_OK == bsp_i2c_eeprom_selftest()) {
        printf("[SELFTEST][PASS] I2C EEPROM read/write\r\n");
    } else {
        pass = 0U;
        printf("[SELFTEST][FAIL] I2C EEPROM read/write\r\n");
    }

    return pass;
}

void auto_probe_init(void)
{
    /* 通道B: 初始化调试串口(USART0 / PB6-TX / PB7-RX / 115200) */
    bsp_debug_uart_init();

    printf("\r\n==== GD32C231 auto-loop boot ====\r\n");
    printf("[BOOT] firmware: my-gd32-project_2026-9-3 (LED sweep)\r\n");
    printf("[BOOT] uart: USART0 PB6/PB7 115200 8N1\r\n");

    g_test_block.test_pass = auto_self_test();
    printf("[SELFTEST] result = %s\r\n",
           (g_test_block.test_pass != 0U) ? "PASS" : "FAIL");
}

void auto_probe_tick(uint32_t led_mask)
{
    g_test_block.heartbeat++;
    g_test_block.led_mask = led_mask;

    /* 带标签的状态行，便于自动化判读(通道B) */
    printf("[TICK] hb=%lu led_mask=0x%02lX pass=%lu\r\n",
           (unsigned long)g_test_block.heartbeat,
           (unsigned long)g_test_block.led_mask,
           (unsigned long)g_test_block.test_pass);
}
