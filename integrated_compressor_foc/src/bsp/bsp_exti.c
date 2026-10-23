/*!
    \file    bsp_exti.c
    \brief   外部中断板级支持包（BSP）实现 —— GPIO 边沿触发中断

    选线/触发/NVIC 配置沿用官方 EXTI 例程做法：
        rcu 使能 GPIO 与 SYSCFG -> gpio 设为输入 -> syscfg_exti_line_config 把某端口的
        某引脚接到对应 EXTI 线 -> exti_init 设触发方式 -> nvic 使能 -> ISR 里清标志并回调。

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

#include "bsp/bsp_exti.h"

/*!
    \brief      边沿触发回调（弱定义），用户可在别处定义同名函数覆盖
    \param[in]  none
    \param[out] none
    \retval     none
*/
__attribute__((weak)) void bsp_exti_callback(void)
{
    /* 默认什么都不做，等待用户重写 */
}

/*!
    \brief      初始化外部中断（引脚输入 + 选线 + 触发方式 + NVIC）
    \param[in]  none
    \param[out] none
    \retval     none
*/
void bsp_exti_init(void)
{
    /* 使能中断引脚所在 GPIO 端口时钟 */
    rcu_periph_clock_enable(BSP_EXTI_GPIO_RCU);
    /* 使能 SYSCFG 时钟：syscfg_exti_line_config（EXTI 选线）依赖 SYSCFG 模块 */
    rcu_periph_clock_enable(RCU_SYSCFG);

    /* 引脚设为输入并按触发方向配上/下拉，保证空闲电平干净、不误触发 */
    gpio_mode_set(BSP_EXTI_PORT, GPIO_MODE_INPUT, BSP_EXTI_PULL, BSP_EXTI_PIN);

    /* 把“某端口的某引脚”接到对应的 EXTI 线（例如 GPIOB 的 PIN2 -> EXTI 线2） */
    syscfg_exti_line_config(BSP_EXTI_SYSCFG_PORT, BSP_EXTI_SYSCFG_PIN);

    /* 配置 EXTI：中断模式(区别于事件模式) + 触发边沿 */
    exti_init(BSP_EXTI_LINE, EXTI_INTERRUPT, BSP_EXTI_TRIG);
    /* 清一次挂起标志，避免使能后立刻误进一次中断 */
    exti_interrupt_flag_clear(BSP_EXTI_LINE);

    /* 使能 NVIC 中断通道 */
    nvic_irq_enable(BSP_EXTI_IRQ, BSP_EXTI_IRQ_PRIO);
}

/*!
    \brief      EXTI 线2 中断服务函数（本模块自带，勿在 gd32c2x1_it.c 再定义同名函数）
    \param[in]  none
    \param[out] none
    \retval     none
    \note       进中断先判并清挂起标志，再回调上层。清标志必须做，否则会反复进中断。
                若改用 EXTI5~15 的引脚，请把函数名改成 EXTI5_9_IRQHandler / EXTI10_15_IRQHandler。
*/
void EXTI2_IRQHandler(void)
{
    if(RESET != exti_interrupt_flag_get(BSP_EXTI_LINE)) {
        exti_interrupt_flag_clear(BSP_EXTI_LINE);
        bsp_exti_callback();
    }
}
