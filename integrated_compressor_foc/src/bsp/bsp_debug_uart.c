/**
 * @file bsp_debug_uart.c
 * @brief printf 调试串口初始化实现。
 */

#include "bsp/bsp_debug_uart.h"

void bsp_debug_uart_init(void)
{
    rcu_periph_clock_enable(BSP_DEBUG_UART_TX_RCU);
    rcu_periph_clock_enable(BSP_DEBUG_UART_RX_RCU);
    rcu_periph_clock_enable(BSP_DEBUG_UART_RCU);

    /* 串口空闲电平为高；上拉可避免外设接管前 RX/TX 悬空。 */
    gpio_af_set(BSP_DEBUG_UART_TX_PORT, BSP_DEBUG_UART_TX_AF, BSP_DEBUG_UART_TX_PIN);
    gpio_mode_set(BSP_DEBUG_UART_TX_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP,
                  BSP_DEBUG_UART_TX_PIN);
    gpio_output_options_set(BSP_DEBUG_UART_TX_PORT, GPIO_OTYPE_PP,
                            GPIO_OSPEED_LEVEL_1, BSP_DEBUG_UART_TX_PIN);

    gpio_af_set(BSP_DEBUG_UART_RX_PORT, BSP_DEBUG_UART_RX_AF, BSP_DEBUG_UART_RX_PIN);
    gpio_mode_set(BSP_DEBUG_UART_RX_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP,
                  BSP_DEBUG_UART_RX_PIN);
    gpio_output_options_set(BSP_DEBUG_UART_RX_PORT, GPIO_OTYPE_PP,
                            GPIO_OSPEED_LEVEL_1, BSP_DEBUG_UART_RX_PIN);

    usart_deinit(BSP_DEBUG_UART_PERIPH);
    usart_baudrate_set(BSP_DEBUG_UART_PERIPH, BSP_DEBUG_UART_BAUDRATE);
    usart_word_length_set(BSP_DEBUG_UART_PERIPH, USART_WL_8BIT);
    usart_stop_bit_set(BSP_DEBUG_UART_PERIPH, USART_STB_1BIT);
    usart_parity_config(BSP_DEBUG_UART_PERIPH, USART_PM_NONE);
    usart_receive_config(BSP_DEBUG_UART_PERIPH, USART_RECEIVE_ENABLE);
    usart_transmit_config(BSP_DEBUG_UART_PERIPH, USART_TRANSMIT_ENABLE);
    usart_enable(BSP_DEBUG_UART_PERIPH);
}
