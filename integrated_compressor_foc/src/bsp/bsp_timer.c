/*!
    \file    bsp_timer.c
    \brief   通用定时器板级支持包（BSP）实现 —— TIMER15 周期性更新中断

    定时器时基/中断配置沿用官方例程 GD32C2x1_Firmware_Library_V1.3.0 /
    Examples/TIMER/TIMER2_timebase 的做法（预分频 + 自动重装载 + 更新中断），
    仅换成不占引脚的 TIMER15，并把“周期到达”通过弱回调交给上层处理。

    \version 2026-07-10, V1.0, GD32C231C8 通用定时器驱动
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

#include "bsp/bsp_timer.h"

/*!
    \brief      周期到达回调（弱定义）。用户在别处定义同名函数即可覆盖它。
    \param[in]  none
    \param[out] none
    \retval     none
    \note       __attribute__((weak)) 让本处成为“默认空实现”：链接时若用户提供了强定义，
                用户版本优先；否则用这个空版本，保证即使不写回调也能正常编译运行。
*/
__attribute__((weak)) void bsp_timer_callback(void)
{
    /* 默认什么都不做，等待用户重写 */
}

/*!
    \brief      初始化并启动定时器：每 period_us 微秒触发一次更新中断
    \param[in]  period_us: 中断周期，单位微秒，取值 1~65535
    \param[out] none
    \retval     none
*/
void bsp_timer_init(uint32_t period_us)
{
    timer_parameter_struct timer_initpara;

    /* 使能定时器时钟 */
    rcu_periph_clock_enable(BSP_TIMER_RCU);

    /* 先复位到已知状态 */
    timer_deinit(BSP_TIMER_PERIPH);

    /* 计数器参数：
       prescaler       预分频 -> 计数频率 = TIMER_CLK/(prescaler+1) = 1MHz（1 计数 1us）
       period          自动重装载值 -> 计满 (period+1) 个计数产生一次更新事件；
                       故 period = period_us - 1，即每 period_us 个 1us 触发一次。
       其余用边沿对齐、向上计数、时钟不再二次分频（与官方 timebase 例程一致）。 */
    timer_struct_para_init(&timer_initpara);
    timer_initpara.prescaler        = BSP_TIMER_PRESCALER;
    timer_initpara.alignedmode      = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection = TIMER_COUNTER_UP;
    timer_initpara.period           = (0U == period_us) ? 0U : (period_us - 1U);
    timer_initpara.clockdivision    = TIMER_CKDIV_DIV1;
    timer_init(BSP_TIMER_PERIPH, &timer_initpara);

    /* 先清一次更新标志，避免刚使能就进一次“上电残留”的中断 */
    timer_interrupt_flag_clear(BSP_TIMER_PERIPH, TIMER_INT_FLAG_UP);
    /* 使能“更新中断”（计数溢出/重装时触发） */
    timer_interrupt_enable(BSP_TIMER_PERIPH, TIMER_INT_UP);

    /* 打开 NVIC 里对应的中断通道 */
    nvic_irq_enable(BSP_TIMER_IRQ, BSP_TIMER_IRQ_PRIO);

    /* 启动计数 */
    timer_enable(BSP_TIMER_PERIPH);
}

/*!
    \brief      启动计数
    \param[in]  none
    \param[out] none
    \retval     none
*/
void bsp_timer_start(void)
{
    timer_enable(BSP_TIMER_PERIPH);
}

/*!
    \brief      停止计数
    \param[in]  none
    \param[out] none
    \retval     none
*/
void bsp_timer_stop(void)
{
    timer_disable(BSP_TIMER_PERIPH);
}

/*!
    \brief      TIMER15 中断服务函数（本模块自带，勿在 gd32c2x1_it.c 再定义同名函数）
    \param[in]  none
    \param[out] none
    \retval     none
    \note       进中断先判并清“更新标志”，再回调上层。清标志必须做，否则会反复进中断。
*/
void TIMER15_IRQHandler(void)
{
    if(SET == timer_interrupt_flag_get(BSP_TIMER_PERIPH, TIMER_INT_FLAG_UP)) {
        timer_interrupt_flag_clear(BSP_TIMER_PERIPH, TIMER_INT_FLAG_UP);
        bsp_timer_callback();
    }
}
