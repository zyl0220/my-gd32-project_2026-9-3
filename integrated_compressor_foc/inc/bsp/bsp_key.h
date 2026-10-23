/*!
    \file    bsp_key.h
    \brief   板载按键板级支持包（BSP）—— 纯标准外设库（SPL）实现的对外接口

    硬件: GD32C231C-EVAL 板载两枚按键，均为高电平有效（按下读到 SET，EXTI 上升沿）
          KEY_WAKEUP -> PA0
          KEY_USER   -> PA4
    实现参考: GD32C2x1_Firmware_Library_V1.3.0 / Examples/GPIO/Keyboard_polling_mode

    \note 本模块采用“轮询 + 软件消抖”，不依赖中断，无需改动 gd32c2x1_it.c；
          移植到其它板子时只需修改下方引脚映射。

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

#ifndef BSP_KEY_H
#define BSP_KEY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "gd32c2x1.h"

/* 移植到其它板子时，只需修改下面的有效电平与引脚映射，bsp_key.c 无需改动 */

/* 按键按下时的 GPIO 电平（GD32C231C-EVAL 为高电平有效；低电平有效的板子改成 RESET，
   同时把下方 BSP_KEY_PULL 改成 GPIO_PUPD_PULLUP） */
#define BSP_KEY_PRESSED_LEVEL            SET
#define BSP_KEY_PULL                    GPIO_PUPD_PULLDOWN

/* 引脚映射：端口 / 引脚 / 端口时钟，三者对应同一物理引脚 */
#define BSP_KEY_WAKEUP_PORT             GPIOA       /* WAKEUP 键 -> PA0 */
#define BSP_KEY_WAKEUP_PIN              GPIO_PIN_0
#define BSP_KEY_WAKEUP_CLK              RCU_GPIOA

#define BSP_KEY_USER_PORT               GPIOA       /* USER 键 -> PA4 */
#define BSP_KEY_USER_PIN                GPIO_PIN_4
#define BSP_KEY_USER_CLK                RCU_GPIOA

/* 软件消抖：连续 BSP_KEY_DEBOUNCE_CNT 次扫描读到同一稳定电平才确认状态翻转。
   按 bsp_key_scan() 的调用周期折算实际消抖时间，例如每 5ms 调一次、阈值 4 即约 20ms。 */
#define BSP_KEY_DEBOUNCE_CNT            4U

/* 板载按键逻辑编号；取值连续从 0 开始，可同时用作配置表下标和循环计数 */
typedef enum {
    BSP_KEY_WAKEUP = 0U,
    BSP_KEY_USER,
    BSP_KEY_NUM                          /*!< 按键总数（哨兵值，非具体按键） */
} bsp_key_id_t;

/* bsp_key_scan() 返回的按键事件 */
typedef enum {
    BSP_KEY_EVENT_NONE = 0U,             /*!< 无事件（电平未发生确认翻转） */
    BSP_KEY_EVENT_DOWN,                  /*!< 一次确认的“按下”沿 */
    BSP_KEY_EVENT_UP                     /*!< 一次确认的“释放”沿 */
} bsp_key_event_t;

/* 初始化某个按键的 GPIO（输入 + 上/下拉） */
void            bsp_key_init(bsp_key_id_t key);
/* 一次性初始化全部按键 */
void            bsp_key_init_all(void);
/* 读取按键的“瞬时”状态（未消抖）：1=当前按下，0=当前松开 */
uint8_t         bsp_key_read_raw(bsp_key_id_t key);
/* 周期性调用以驱动消抖状态机，返回本次扫描产生的按下/释放事件 */
bsp_key_event_t bsp_key_scan(bsp_key_id_t key);
/* 查询按键“消抖后”的稳定状态：1=已确认按下，0=已确认松开 */
uint8_t         bsp_key_is_pressed(bsp_key_id_t key);

#ifdef __cplusplus
}
#endif

#endif /* BSP_KEY_H */
