/*!
    \file    bsp_exti.h
    \brief   外部中断板级支持包（BSP）—— 某个 GPIO 上升/下降沿触发中断

    硬件: 默认 PB2 作为中断输入（EXTI 线 2），上升沿触发。
    实现参考: GD32C2x1_Firmware_Library_V1.3.0 / Examples/EXTI（syscfg 选线 + exti_init + NVIC）

    ================================ 选脚说明 =================================
    - EXTI 的“线号”等于引脚号：PB2 对应 EXTI 线 2，中断向量是 EXTI2_IRQHandler。
    - 选 PB2 是因为：PB2 在本工程里空闲，且落在“独立中断向量”的 EXTI0~EXTI4 段，
      ISR 一对一，最简单清晰（EXTI5 及以上是 EXTI5_9 / EXTI10_15 合并向量，需在
      一个 ISR 里再分辨是哪条线）。
    - 若要改到别的引脚：同时改下面 4 个宏，并把 bsp_exti.c 里的 ISR 名从
      EXTI2_IRQHandler 改成新引脚对应的向量名（0~4 各自独立，5~9 用 EXTI5_9_IRQHandler，
      10~15 用 EXTI10_15_IRQHandler）。
    - 注意别和板上按键 EXTI 线撞号：本板按键 PA0/PA4 是“轮询”方式、并不占用 EXTI 中断，
      但若你把本模块也放到 PA0/PA4，会与同号 EXTI 线语义重叠，建议避开。PB2 不冲突。
    ==========================================================================

    使用方法（用户只需重写回调）：
        void bsp_exti_callback(void) {    // 覆盖弱定义
            // 边沿到来时做的事，务必简短
        }
        ...
        bsp_exti_init();                  // 之后 PB2 上升沿即触发回调

    \version 2026-07-10, V1.0, GD32C231C8 外部中断驱动
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

#ifndef BSP_EXTI_H
#define BSP_EXTI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "gd32c2x1.h"
#include <stdint.h>

/* ---- 引脚与 EXTI 线资源映射（换引脚改这里，并同步改 bsp_exti.c 的 ISR 名）----
   BSP_EXTI_GPIO_RCU   引脚所在端口时钟
   BSP_EXTI_PORT/PIN   引脚
   BSP_EXTI_SYSCFG_PORT/PIN  syscfg 选线用的端口/引脚源（必须和上面的引脚对应）
   BSP_EXTI_LINE       EXTI 线号（= 引脚号）
   BSP_EXTI_IRQ        NVIC 中断号 */
#define BSP_EXTI_GPIO_RCU      RCU_GPIOB
#define BSP_EXTI_PORT          GPIOB
#define BSP_EXTI_PIN           GPIO_PIN_2
#define BSP_EXTI_PULL          GPIO_PUPD_PULLDOWN   /* 上升沿触发配下拉，空闲拉低更干净 */
#define BSP_EXTI_SYSCFG_PORT   EXTI_SOURCE_GPIOB
#define BSP_EXTI_SYSCFG_PIN    EXTI_SOURCE_PIN2
#define BSP_EXTI_LINE          EXTI_2
#define BSP_EXTI_IRQ           EXTI2_IRQn

/* 触发边沿：EXTI_TRIG_RISING（上升沿）/ EXTI_TRIG_FALLING（下降沿）/ EXTI_TRIG_BOTH（双边沿） */
#define BSP_EXTI_TRIG          EXTI_TRIG_RISING

/* 中断抢占优先级（数值越小越高） */
#define BSP_EXTI_IRQ_PRIO      2U

/* ---- 对外 API ---- */
/* 初始化外部中断：配置引脚为输入 + syscfg 选线 + EXTI 触发方式 + NVIC 使能 */
void bsp_exti_init(void);

/* 边沿触发时被中断调用的回调，弱定义。用户在自己文件里重写同名函数即可，务必简短。 */
void bsp_exti_callback(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_EXTI_H */
