/*!
    \file    bsp_key.c
    \brief   板载按键板级支持包（BSP）—— 纯标准外设库（SPL）实现

    采用“轮询 + 计数器消抖”的状态机：每次 bsp_key_scan() 采样一次电平，
    连续 BSP_KEY_DEBOUNCE_CNT 次读到与当前稳定状态相反的电平后，才确认翻转
    并产生一次 DOWN / UP 事件。不使用中断，无需改动 gd32c2x1_it.c。

    \version 2026-06-22, V1.0, GD32C231C-EVAL 板级按键驱动
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

#include "bsp/bsp_key.h"

/* 单个按键的硬件描述 */
typedef struct {
    uint32_t        port;                /*!< GPIO 端口基地址 */
    uint32_t        pin;                 /*!< GPIO 引脚掩码 */
    rcu_periph_enum clock;               /*!< GPIO 端口时钟 */
} key_hw_t;

/* 单个按键的消抖运行时状态 */
typedef struct {
    uint8_t stable;                      /*!< 已确认（消抖后）的状态：1=按下 0=松开 */
    uint8_t counter;                     /*!< 与 stable 相反电平的连续计数 */
} key_state_t;

/* 按键引脚映射表，下标与 bsp_key_id_t 一一对应，是驱动的唯一数据源 */
static const key_hw_t g_key_hw[BSP_KEY_NUM] = {
    {BSP_KEY_WAKEUP_PORT, BSP_KEY_WAKEUP_PIN, BSP_KEY_WAKEUP_CLK},
    {BSP_KEY_USER_PORT,   BSP_KEY_USER_PIN,   BSP_KEY_USER_CLK},
};

/* 各按键消抖状态，初值为“松开” */
static key_state_t g_key_state[BSP_KEY_NUM];

/*!
    \brief      初始化某个按键的 GPIO（输入 + 上/下拉）
    \param[in]  key: 要初始化的按键，取值为 bsp_key_id_t
      \arg        BSP_KEY_WAKEUP, BSP_KEY_USER
    \param[out] none
    \retval     none
*/
void bsp_key_init(bsp_key_id_t key)
{
    const key_hw_t *hw;

    if(key >= BSP_KEY_NUM) {
        return;
    }
    hw = &g_key_hw[key];

    /* GD32 外设使用前必须先开时钟 */
    rcu_periph_clock_enable(hw->clock);
    gpio_mode_set(hw->port, GPIO_MODE_INPUT, BSP_KEY_PULL, hw->pin);

    /* 复位该按键的消抖状态为“松开” */
    g_key_state[key].stable  = 0U;
    g_key_state[key].counter = 0U;
}

/*!
    \brief      一次性初始化全部按键
    \param[in]  none
    \param[out] none
    \retval     none
*/
void bsp_key_init_all(void)
{
    bsp_key_id_t key;

    for(key = BSP_KEY_WAKEUP; key < BSP_KEY_NUM; key++) {
        bsp_key_init(key);
    }
}

/*!
    \brief      读取按键的“瞬时”状态（未消抖）
    \param[in]  key: 要读取的按键，取值为 bsp_key_id_t
      \arg        BSP_KEY_WAKEUP, BSP_KEY_USER
    \param[out] none
    \retval     1=当前按下，0=当前松开（或参数非法）
*/
uint8_t bsp_key_read_raw(bsp_key_id_t key)
{
    if(key >= BSP_KEY_NUM) {
        return 0U;
    }
    return (BSP_KEY_PRESSED_LEVEL ==
            gpio_input_bit_get(g_key_hw[key].port, g_key_hw[key].pin)) ? 1U : 0U;
}

/*!
    \brief      周期性调用以驱动消抖状态机
    \param[in]  key: 要扫描的按键，取值为 bsp_key_id_t
      \arg        BSP_KEY_WAKEUP, BSP_KEY_USER
    \param[out] none
    \retval     本次扫描产生的事件，取值为 bsp_key_event_t
                BSP_KEY_EVENT_NONE / BSP_KEY_EVENT_DOWN / BSP_KEY_EVENT_UP
*/
bsp_key_event_t bsp_key_scan(bsp_key_id_t key)
{
    uint8_t      raw;
    key_state_t *st;

    if(key >= BSP_KEY_NUM) {
        return BSP_KEY_EVENT_NONE;
    }
    st  = &g_key_state[key];
    raw = bsp_key_read_raw(key);

    if(raw == st->stable) {
        /* 与稳定状态一致：抖动计数清零，无事件 */
        st->counter = 0U;
        return BSP_KEY_EVENT_NONE;
    }

    /* 与稳定状态相反：累计，达到阈值才确认翻转 */
    st->counter++;
    if(st->counter < BSP_KEY_DEBOUNCE_CNT) {
        return BSP_KEY_EVENT_NONE;
    }

    st->counter = 0U;
    st->stable  = raw;
    return (raw != 0U) ? BSP_KEY_EVENT_DOWN : BSP_KEY_EVENT_UP;
}

/*!
    \brief      查询按键“消抖后”的稳定状态
    \param[in]  key: 要查询的按键，取值为 bsp_key_id_t
      \arg        BSP_KEY_WAKEUP, BSP_KEY_USER
    \param[out] none
    \retval     1=已确认按下，0=已确认松开（或参数非法）
*/
uint8_t bsp_key_is_pressed(bsp_key_id_t key)
{
    if(key >= BSP_KEY_NUM) {
        return 0U;
    }
    return g_key_state[key].stable;
}
