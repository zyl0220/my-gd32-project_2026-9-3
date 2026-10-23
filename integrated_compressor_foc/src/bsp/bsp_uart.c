/*!
    \file    bsp_uart.c
    \brief   通用 UART 板级支持包（BSP）实现 —— 阻塞式收发

    引脚/控制器配置与收发时序沿用官方例程 GD32C2x1_Firmware_Library_V1.3.0 /
    Examples/USART 的做法（TBE 判发送空、RBNE 判接收非空、TC 判发送完成），
    仅做模块化封装, 不改动寄存器操作序列。

    \version 2026-07-10, V1.0, GD32C231C8 通用 UART 驱动
*/

/*
    Copyright (c) 2026, GigaDevice Semiconductor Inc.

    Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this
       list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.
    3. Neither the name of the copyright holder nor the names of its contributors
       may be used to endorse or promote products derived from this software without
       specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.
*/

#include "bsp/bsp_uart.h"
#include "systick.h"
#include <stddef.h>     /* NULL */

/*!
    \brief      初始化 UART 引脚 + 控制器（默认 8N1）
    \param[in]  none
    \param[out] none
    \retval     none
*/
void bsp_uart_init(void)
{
    /* 使能 GPIO 与 USART 外设时钟。
       TX/RX 可能落在不同 GPIO 端口，故按宏分别使能；同一端口时重复使能同一时钟无副作用。 */
    rcu_periph_clock_enable(BSP_UART_TX_RCU);
    rcu_periph_clock_enable(BSP_UART_RX_RCU);
    rcu_periph_clock_enable(BSP_UART_RCU);

    /* TX 引脚：选复用功能号(AF) -> 设为复用模式 -> 推挽输出。
       串口空闲时线路应为高电平，这里加上拉让引脚在外设接管前也保持高，避免上电毛刺被误判为起始位。
       推挽(PP)是为了主动驱动高/低电平，保证边沿速率。 */
    gpio_af_set(BSP_UART_TX_PORT, BSP_UART_TX_AF, BSP_UART_TX_PIN);
    gpio_mode_set(BSP_UART_TX_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, BSP_UART_TX_PIN);
    gpio_output_options_set(BSP_UART_TX_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_LEVEL_1, BSP_UART_TX_PIN);

    /* RX 引脚：同样切到复用功能并加上拉。
       上拉保证对端未连接/悬空时 RX 读到稳定高电平(空闲态)，不会因浮空乱跳触发误接收。 */
    gpio_af_set(BSP_UART_RX_PORT, BSP_UART_RX_AF, BSP_UART_RX_PIN);
    gpio_mode_set(BSP_UART_RX_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, BSP_UART_RX_PIN);
    gpio_output_options_set(BSP_UART_RX_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_LEVEL_1, BSP_UART_RX_PIN);

    /* 控制器配置顺序：先复位到已知状态，再逐项设参数，最后使能。
       注意所有参数必须在 usart_enable() 之前设好——使能后再改部分寄存器不一定生效。 */
    usart_deinit(BSP_UART_PERIPH);                                    /* 复位到上电默认值，清掉可能的残留配置 */
    usart_baudrate_set(BSP_UART_PERIPH, BSP_UART_BAUDRATE);           /* 波特率，库内部按当前 USART 时钟自动算分频 */
    usart_word_length_set(BSP_UART_PERIPH, USART_WL_8BIT);           /* 8 数据位 */
    usart_stop_bit_set(BSP_UART_PERIPH, USART_STB_1BIT);            /* 1 停止位 */
    usart_parity_config(BSP_UART_PERIPH, USART_PM_NONE);           /* 无校验 —— 合起来就是常说的 8N1 */
    usart_receive_config(BSP_UART_PERIPH, USART_RECEIVE_ENABLE);    /* 使能接收器(REN) */
    usart_transmit_config(BSP_UART_PERIPH, USART_TRANSMIT_ENABLE);  /* 使能发送器(TEN) */
    usart_enable(BSP_UART_PERIPH);                                   /* 使能 USART，配置生效 */
}

/*!
    \brief      阻塞发送单字节
    \param[in]  byte: 待发送字节
    \param[out] none
    \retval     none
*/
void bsp_uart_send_byte(uint8_t byte)
{
    /* 等发送数据缓冲空 -> 写入 -> 等本字节移位完成 */
    while(RESET == usart_flag_get(BSP_UART_PERIPH, USART_FLAG_TBE)) {
    }
    usart_data_transmit(BSP_UART_PERIPH, (uint16_t)byte);
    while(RESET == usart_flag_get(BSP_UART_PERIPH, USART_FLAG_TC)) {
    }
}

/*!
    \brief      阻塞发送缓冲区
    \param[in]  buffer: 数据指针
    \param[in]  len: 字节数
    \param[out] none
    \retval     none
*/
void bsp_uart_send_buffer(const uint8_t *buffer, uint16_t len)
{
    uint16_t i;

    if(NULL == buffer) {
        return;
    }
    for(i = 0U; i < len; i++) {
        bsp_uart_send_byte(buffer[i]);
    }
}

/*!
    \brief      阻塞发送以 '\0' 结尾的字符串
    \param[in]  str: 字符串指针
    \param[out] none
    \retval     none
*/
void bsp_uart_send_string(const char *str)
{
    if(NULL == str) {
        return;
    }
    while('\0' != *str) {
        bsp_uart_send_byte((uint8_t)*str);
        str++;
    }
}

/*!
    \brief      阻塞接收单字节（无限等待）
    \param[in]  none
    \param[out] none
    \retval     收到的字节
*/
uint8_t bsp_uart_recv_byte(void)
{
    while(RESET == usart_flag_get(BSP_UART_PERIPH, USART_FLAG_RBNE)) {
    }
    return (uint8_t)usart_data_receive(BSP_UART_PERIPH);
}

/*!
    \brief      带超时接收单字节
    \param[in]  timeout_ms: 最长等待毫秒数
    \param[out] out: 收到的字节写入此处（仅在返回 OK 时有效）
    \retval     BSP_UART_OK / BSP_UART_TIMEOUT
*/
uint8_t bsp_uart_recv_byte_timeout(uint8_t *out, uint32_t timeout_ms)
{
    uint32_t elapsed = 0U;

    if(NULL == out) {
        return BSP_UART_TIMEOUT;
    }
    /* 以 1ms 为粒度轮询 RBNE，依赖 systick_config() 已在 main 中调用 */
    while(RESET == usart_flag_get(BSP_UART_PERIPH, USART_FLAG_RBNE)) {
        if(elapsed >= timeout_ms) {
            return BSP_UART_TIMEOUT;
        }
        delay_1ms(1U);
        elapsed++;
    }
    *out = (uint8_t)usart_data_receive(BSP_UART_PERIPH);
    return BSP_UART_OK;
}

/*!
    \brief      查询是否有数据可读
    \param[in]  none
    \param[out] none
    \retval     1=有数据, 0=无数据
*/
uint8_t bsp_uart_data_available(void)
{
    return (RESET != usart_flag_get(BSP_UART_PERIPH, USART_FLAG_RBNE)) ? 1U : 0U;
}
