/*!
    \file    bsp_led.h
    \brief   板载 LED 板级支持包（BSP）—— 纯标准外设库（SPL）实现的对外接口

    \version 2026-06-09, V1.0, GD32C231C-EVAL 板级 LED 驱动
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

#ifndef BSP_LED_H
#define BSP_LED_H

#ifdef __cplusplus
extern "C" {
#endif

#include "gd32c2x1.h"

/* 移植到其它板子时，只需修改下面的点亮电平和引脚映射，bsp_led.c 无需改动 */

/* LED 点亮/熄灭电平（GD32C231C-EVAL 为高电平点亮；低电平点亮的板子把两者对调即可） */
#define BSP_LED_ON_LEVEL                 SET
#define BSP_LED_OFF_LEVEL                RESET

/* 引脚映射：端口 / 引脚 / 端口时钟，三者对应同一物理引脚 */
#define BSP_LED1_PORT                    GPIOA       /* LED1 -> PA15 */
#define BSP_LED1_PIN                     GPIO_PIN_15
#define BSP_LED1_CLK                     RCU_GPIOA

#define BSP_LED2_PORT                    GPIOD       /* LED2 -> PD1 */
#define BSP_LED2_PIN                     GPIO_PIN_1
#define BSP_LED2_CLK                     RCU_GPIOD

#define BSP_LED3_PORT                    GPIOD       /* LED3 -> PD3 */
#define BSP_LED3_PIN                     GPIO_PIN_3
#define BSP_LED3_CLK                     RCU_GPIOD

#define BSP_LED4_PORT                    GPIOB       /* LED4 -> PB4 */
#define BSP_LED4_PIN                     GPIO_PIN_4
#define BSP_LED4_CLK                     RCU_GPIOB

/* 板载 LED 逻辑编号；取值连续从 0 开始，可同时用作配置表下标和循环计数。
   加 BSP_ 前缀以避免与保留的 gd32c231c_eval.h 中的 LED1~LED4 枚举冲突。 */
typedef enum {
    BSP_LED1 = 0U,
    BSP_LED2,
    BSP_LED3,
    BSP_LED4,
    BSP_LED_NUM                          /*!< LED 总数（哨兵值，非具体 LED） */
} bsp_led_id_t;

/* 初始化某个 LED 的 GPIO，并使其初始处于熄灭状态 */
void bsp_led_init(bsp_led_id_t led);
/* 点亮指定 LED */
void bsp_led_on(bsp_led_id_t led);
/* 熄灭指定 LED */
void bsp_led_off(bsp_led_id_t led);
/* 翻转指定 LED 的亮灭状态 */
void bsp_led_toggle(bsp_led_id_t led);

#ifdef __cplusplus
}
#endif

#endif /* BSP_LED_H */
