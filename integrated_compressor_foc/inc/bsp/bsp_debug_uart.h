/**
 * @file bsp_debug_uart.h
 * @brief printf 调试串口的项目级引脚配置。
 *
 * USART0 的 printf 重定向仍由厂商 EVAL 文件提供；本 BSP 只接管控制器和
 * GPIO 初始化，使引脚映射不会因资源管理器重新生成 EVAL 文件而丢失。
 */

#ifndef BSP_DEBUG_UART_H
#define BSP_DEBUG_UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include "gd32c2x1.h"

#define BSP_DEBUG_UART_PERIPH       USART0
#define BSP_DEBUG_UART_RCU          RCU_USART0
#define BSP_DEBUG_UART_BAUDRATE     115200U

#define BSP_DEBUG_UART_TX_PORT      GPIOB
#define BSP_DEBUG_UART_TX_PIN       GPIO_PIN_6
#define BSP_DEBUG_UART_TX_RCU       RCU_GPIOB
#define BSP_DEBUG_UART_TX_AF        GPIO_AF_0

#define BSP_DEBUG_UART_RX_PORT      GPIOB
#define BSP_DEBUG_UART_RX_PIN       GPIO_PIN_7
#define BSP_DEBUG_UART_RX_RCU       RCU_GPIOB
#define BSP_DEBUG_UART_RX_AF        GPIO_AF_0

/** 初始化 USART0 PB6/PB7，115200 8N1，供 printf 和诊断日志使用。 */
void bsp_debug_uart_init(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_DEBUG_UART_H */
