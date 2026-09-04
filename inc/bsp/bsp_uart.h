/*!
    \file    bsp_uart.h
    \brief   通用 UART 板级支持包（BSP）—— 阻塞式收发，供随手调用

    硬件: USART1, TX=PA2 (AF2) / RX=PA3 (AF1), 默认 115200 8N1
    实现参考: GD32C2x1_Firmware_Library_V1.3.0 / Examples/USART

    \note 本模块与调试串口(USART0 / PA9-PA10 / printf, 见 auto_probe.c)相互独立，
          互不影响。需要一路“干净的、可自由收发的”串口时用本模块即可。
          换 USART 实例 / 换引脚只改下方宏, bsp_uart.c 无需改动。

    使用示例:
        bsp_uart_init();                         // main 早期调用一次
        bsp_uart_send_string("hello\r\n");       // 发字符串
        bsp_uart_send_byte(0x55);                // 发单字节
        uint8_t b = bsp_uart_recv_byte();        // 阻塞收一字节
        uint8_t rx;
        if(BSP_UART_OK == bsp_uart_recv_byte_timeout(&rx, 100)) { ... } // 100ms 超时收

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

#ifndef BSP_UART_H
#define BSP_UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include "gd32c2x1.h"
#include <stdint.h>

/* ---- 硬件资源映射（换 USART / 换引脚改这里；AF 号见数据手册 Rev1.6 复用功能表 表2-2）----
   USART1 默认: TX=PA2(AF1), RX=PA3(AF1)。 */
#define BSP_UART_PERIPH        USART1
#define BSP_UART_RCU           RCU_USART1
#define BSP_UART_BAUDRATE      115200U

#define BSP_UART_TX_PORT       GPIOA
#define BSP_UART_TX_PIN        GPIO_PIN_2
#define BSP_UART_TX_AF         GPIO_AF_1
#define BSP_UART_TX_RCU        RCU_GPIOA

#define BSP_UART_RX_PORT       GPIOA
#define BSP_UART_RX_PIN        GPIO_PIN_3
#define BSP_UART_RX_AF         GPIO_AF_1
#define BSP_UART_RX_RCU        RCU_GPIOA

/* 返回码 */
#define BSP_UART_OK            0U
#define BSP_UART_TIMEOUT       1U

/* ---- 对外 API ---- */
/* 初始化 UART 引脚 + 控制器（BSP_UART_BAUDRATE, 8 数据位 / 1 停止位 / 无校验），main 早期调用一次 */
void    bsp_uart_init(void);
/* 阻塞发送单字节 */
void    bsp_uart_send_byte(uint8_t byte);
/* 阻塞发送 len 字节缓冲区 */
void    bsp_uart_send_buffer(const uint8_t *buffer, uint16_t len);
/* 阻塞发送以 '\0' 结尾的字符串 */
void    bsp_uart_send_string(const char *str);
/* 阻塞接收单字节（无限等待，直到收到数据） */
uint8_t bsp_uart_recv_byte(void);
/* 带超时接收单字节：收到写入 *out 返回 BSP_UART_OK；timeout_ms 内无数据返回 BSP_UART_TIMEOUT */
uint8_t bsp_uart_recv_byte_timeout(uint8_t *out, uint32_t timeout_ms);
/* 查询是否有数据可读（1=有, 0=无），用于非阻塞轮询 */
uint8_t bsp_uart_data_available(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_UART_H */
