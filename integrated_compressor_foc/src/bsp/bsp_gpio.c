/*!
    \file    bsp_gpio.c
    \brief   通用 GPIO 板级支持包（BSP）实现 —— 任意引脚的数字量输入/输出

    薄封装，直接调用标准外设库(SPL)的 gpio_* 接口。把“使能时钟 + 设模式 + 设输出选项”
    三步合成一个 init，读写翻转各一行，减少上层重复样板代码。

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

#include "bsp/bsp_gpio.h"

/*!
    \brief      把引脚配置成推挽输出
    \param[in]  rcu: 引脚所在端口的时钟枚举，如 RCU_GPIOC
    \param[in]  gpio_port: 端口基址，如 GPIOC
    \param[in]  pin: 引脚掩码，可“或”多个，如 GPIO_PIN_6 | GPIO_PIN_7
    \param[out] none
    \retval     none
*/
void bsp_gpio_init_output(rcu_periph_enum rcu, uint32_t gpio_port, uint32_t pin)
{
    rcu_periph_clock_enable(rcu);
    /* 输出模式无需上下拉；推挽(PP)可主动驱动高低电平；速率取常规档即可 */
    gpio_mode_set(gpio_port, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, pin);
    gpio_output_options_set(gpio_port, GPIO_OTYPE_PP, GPIO_OSPEED_LEVEL_1, pin);
}

/*!
    \brief      把引脚配置成输入
    \param[in]  rcu: 引脚所在端口的时钟枚举
    \param[in]  gpio_port: 端口基址
    \param[in]  pin: 引脚掩码
    \param[in]  pull: 上下拉 GPIO_PUPD_NONE / GPIO_PUPD_PULLUP / GPIO_PUPD_PULLDOWN
    \param[out] none
    \retval     none
*/
void bsp_gpio_init_input(rcu_periph_enum rcu, uint32_t gpio_port, uint32_t pin, uint32_t pull)
{
    rcu_periph_clock_enable(rcu);
    gpio_mode_set(gpio_port, GPIO_MODE_INPUT, pull, pin);
}

/*!
    \brief      输出高电平
    \param[in]  gpio_port: 端口基址
    \param[in]  pin: 引脚掩码
    \param[out] none
    \retval     none
*/
void bsp_gpio_high(uint32_t gpio_port, uint32_t pin)
{
    gpio_bit_set(gpio_port, pin);
}

/*!
    \brief      输出低电平
    \param[in]  gpio_port: 端口基址
    \param[in]  pin: 引脚掩码
    \param[out] none
    \retval     none
*/
void bsp_gpio_low(uint32_t gpio_port, uint32_t pin)
{
    gpio_bit_reset(gpio_port, pin);
}

/*!
    \brief      按 level 输出电平
    \param[in]  gpio_port: 端口基址
    \param[in]  pin: 引脚掩码
    \param[in]  level: 非 0 输出高，0 输出低
    \param[out] none
    \retval     none
*/
void bsp_gpio_write(uint32_t gpio_port, uint32_t pin, uint8_t level)
{
    gpio_bit_write(gpio_port, pin, (0U != level) ? SET : RESET);
}

/*!
    \brief      翻转输出电平
    \param[in]  gpio_port: 端口基址
    \param[in]  pin: 引脚掩码
    \param[out] none
    \retval     none
*/
void bsp_gpio_toggle(uint32_t gpio_port, uint32_t pin)
{
    gpio_bit_toggle(gpio_port, pin);
}

/*!
    \brief      读单个引脚的输入电平
    \param[in]  gpio_port: 端口基址
    \param[in]  pin: 单个引脚掩码（不要“或”多个）
    \param[out] none
    \retval     1=高电平, 0=低电平
*/
uint8_t bsp_gpio_read(uint32_t gpio_port, uint32_t pin)
{
    return (RESET != gpio_input_bit_get(gpio_port, pin)) ? 1U : 0U;
}
