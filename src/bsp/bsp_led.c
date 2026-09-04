/*!
    \file    bsp_led.c
    \brief   板载 LED 板级支持包（BSP）—— 纯标准外设库（SPL）实现

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

#include "bsp/bsp_led.h"

/* 单个 LED 的硬件描述 */
typedef struct {
    uint32_t        port;                /*!< GPIO 端口基地址 */
    uint32_t        pin;                 /*!< GPIO 引脚掩码 */
    rcu_periph_enum clock;               /*!< GPIO 端口时钟 */
} led_hw_t;

/* LED 引脚映射表，下标与 bsp_led_id_t 一一对应，是驱动的唯一数据源 */
static const led_hw_t g_led_hw[BSP_LED_NUM] = {
    {BSP_LED1_PORT, BSP_LED1_PIN, BSP_LED1_CLK},
    {BSP_LED2_PORT, BSP_LED2_PIN, BSP_LED2_CLK},
    {BSP_LED3_PORT, BSP_LED3_PIN, BSP_LED3_CLK},
    {BSP_LED4_PORT, BSP_LED4_PIN, BSP_LED4_CLK},
};

/*!
    \brief      初始化某个 LED 的 GPIO，并使其初始处于熄灭状态
    \param[in]  led: 要初始化的 LED，取值为 bsp_led_id_t
      \arg        BSP_LED1, BSP_LED2, BSP_LED3, BSP_LED4
    \param[out] none
    \retval     none
*/
void bsp_led_init(bsp_led_id_t led)
{
    const led_hw_t *hw;

    if(led >= BSP_LED_NUM) {
        return;
    }
    hw = &g_led_hw[led];

    /* GD32 外设使用前必须先开时钟 */
    rcu_periph_clock_enable(hw->clock);
    gpio_mode_set(hw->port, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, hw->pin);
    gpio_output_options_set(hw->port, GPIO_OTYPE_PP, GPIO_OSPEED_LEVEL_1, hw->pin);

    gpio_bit_write(hw->port, hw->pin, BSP_LED_OFF_LEVEL);
}

/*!
    \brief      点亮指定 LED
    \param[in]  led: 要点亮的 LED，取值为 bsp_led_id_t
      \arg        BSP_LED1, BSP_LED2, BSP_LED3, BSP_LED4
    \param[out] none
    \retval     none
*/
void bsp_led_on(bsp_led_id_t led)
{
    if(led >= BSP_LED_NUM) {
        return;
    }
    gpio_bit_write(g_led_hw[led].port, g_led_hw[led].pin, BSP_LED_ON_LEVEL);
}

/*!
    \brief      熄灭指定 LED
    \param[in]  led: 要熄灭的 LED，取值为 bsp_led_id_t
      \arg        BSP_LED1, BSP_LED2, BSP_LED3, BSP_LED4
    \param[out] none
    \retval     none
*/
void bsp_led_off(bsp_led_id_t led)
{
    if(led >= BSP_LED_NUM) {
        return;
    }
    gpio_bit_write(g_led_hw[led].port, g_led_hw[led].pin, BSP_LED_OFF_LEVEL);
}

/*!
    \brief      翻转指定 LED 的亮灭状态
    \param[in]  led: 要翻转的 LED，取值为 bsp_led_id_t
      \arg        BSP_LED1, BSP_LED2, BSP_LED3, BSP_LED4
    \param[out] none
    \retval     none
*/
void bsp_led_toggle(bsp_led_id_t led)
{
    if(led >= BSP_LED_NUM) {
        return;
    }
    gpio_bit_toggle(g_led_hw[led].port, g_led_hw[led].pin);
}
