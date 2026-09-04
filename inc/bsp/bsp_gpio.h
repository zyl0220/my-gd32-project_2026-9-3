/*!
    \file    bsp_gpio.h
    \brief   通用 GPIO 板级支持包（BSP）—— 任意引脚的数字量输入/输出封装

    与其它 bsp_* 不同，本模块不绑定固定引脚：端口/引脚/时钟都由调用者传入，
    因此天然不会和别的外设抢脚。适合驱动继电器、蜂鸣器、使能脚、状态灯、读开关量等
    “一个引脚一件事”的简单场合，省去每次都写 rcu/gpio_mode_set/output_options 三件套。

    使用示例（把 PC6 当输出翻转、PC7 当输入读电平）:
        bsp_gpio_init_output(RCU_GPIOC, GPIOC, GPIO_PIN_6);   // 推挽输出
        bsp_gpio_high(GPIOC, GPIO_PIN_6);                     // 输出高
        bsp_gpio_toggle(GPIOC, GPIO_PIN_6);                   // 翻转
        bsp_gpio_init_input(RCU_GPIOC, GPIOC, GPIO_PIN_7, GPIO_PUPD_PULLUP);
        uint8_t lvl = bsp_gpio_read(GPIOC, GPIO_PIN_7);       // 读输入电平 0/1

    \note pin 参数支持“或”多个引脚一次配置，如 (GPIO_PIN_6 | GPIO_PIN_7)。
          读引脚 bsp_gpio_read() 只对单个引脚有意义。

    \version 2026-07-10, V1.0, GD32C231C8 通用 GPIO 驱动
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

#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "gd32c2x1.h"
#include <stdint.h>

/* ---- 对外 API ---- */
/* 把引脚配置成推挽输出（含使能端口时钟），初始电平不确定，需再调 high/low 设定 */
void    bsp_gpio_init_output(rcu_periph_enum rcu, uint32_t gpio_port, uint32_t pin);
/* 把引脚配置成输入（含使能端口时钟）。pull 取 GPIO_PUPD_NONE/GPIO_PUPD_PULLUP/GPIO_PUPD_PULLDOWN */
void    bsp_gpio_init_input(rcu_periph_enum rcu, uint32_t gpio_port, uint32_t pin, uint32_t pull);
/* 输出高电平 */
void    bsp_gpio_high(uint32_t gpio_port, uint32_t pin);
/* 输出低电平 */
void    bsp_gpio_low(uint32_t gpio_port, uint32_t pin);
/* 按 level 输出电平：level!=0 输出高，否则输出低 */
void    bsp_gpio_write(uint32_t gpio_port, uint32_t pin, uint8_t level);
/* 翻转输出电平 */
void    bsp_gpio_toggle(uint32_t gpio_port, uint32_t pin);
/* 读单个引脚的输入电平，返回 1(高) / 0(低) */
uint8_t bsp_gpio_read(uint32_t gpio_port, uint32_t pin);

#ifdef __cplusplus
}
#endif

#endif /* BSP_GPIO_H */
