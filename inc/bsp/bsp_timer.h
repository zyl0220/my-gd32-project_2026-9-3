/*!
    \file    bsp_timer.h
    \brief   通用定时器板级支持包（BSP）—— 周期性更新中断，供“定时执行某事”

    硬件: TIMER15（基本定时器，不占任何外部引脚），产生周期性更新(UP)中断。
    实现参考: GD32C2x1_Firmware_Library_V1.3.0 / Examples/TIMER/TIMER2_timebase

    与 bsp_pwm(占用 TIMER2/PB4) 相互独立、互不影响；不引出引脚，纯做时基。
    典型用途：固定周期扫描按键、喂狗、软件计时、状态机 tick、周期采样等。

    工作方式：
        - 计数时钟先被预分频到 1MHz（1 个计数 = 1us），故周期以“微秒”为单位设置，
          分辨率 1us，单次最大 65535us（≈65.5ms，受 16 位计数器限制）。
        - 需要更长周期(如 1s)：增大 BSP_TIMER_PRESCALER 使计数变慢，或在回调里自行累计。
        - 每到一个周期触发一次中断，中断里调用弱回调 bsp_timer_callback()。

    使用方法（用户只需重写回调，不用改本模块）：
        void bsp_timer_callback(void) {   // 覆盖弱定义
            bsp_led_toggle(BSP_LED1);     // 每个周期做的事
        }
        ...
        bsp_timer_init(1000U);            // 每 1000us=1ms 触发一次

    \note 本模块在 bsp_timer.c 里实现了 TIMER15_IRQHandler，未在 gd32c2x1_it.c 中重复定义，
          不会符号冲突。中断优先级见 BSP_TIMER_IRQ_PRIO。

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

#ifndef BSP_TIMER_H
#define BSP_TIMER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "gd32c2x1.h"
#include <stdint.h>

/* ---- 定时器资源映射（换定时器改这三处，注意 ISR 名要同步改成对应的 TIMERx_IRQHandler）---- */
#define BSP_TIMER_PERIPH       TIMER15
#define BSP_TIMER_RCU          RCU_TIMER15
#define BSP_TIMER_IRQ          TIMER15_IRQn

/* 计数时钟预分频：把定时器时钟分频到 1MHz（1 计数=1us）。
   本工程 TIMER 时钟 48MHz，48 分频 -> 1MHz，故预分频寄存器值 = 48-1 = 47。
   若系统时钟不是 48MHz，改这里让计数频率保持 1MHz，period 的“微秒”含义才准确。 */
#define BSP_TIMER_PRESCALER    47U

/* 中断抢占优先级（Cortex-M23 只有抢占优先级，数值越小越高） */
#define BSP_TIMER_IRQ_PRIO     1U

/* ---- 对外 API ---- */
/* 初始化并启动定时器：每 period_us 微秒触发一次中断（1~65535us）。
   触发时会调用 bsp_timer_callback()。 */
void bsp_timer_init(uint32_t period_us);
/* 启动计数（bsp_timer_init 内部已启动，停止后可用它重新启动） */
void bsp_timer_start(void);
/* 停止计数（暂停周期中断，配置保留） */
void bsp_timer_stop(void);

/* 周期到达时被中断调用的回调。默认是弱空实现，用户在自己文件里重写同名函数即可，
   无需修改本模块。务必保持简短、不要在里面做阻塞/长耗时操作。 */
void bsp_timer_callback(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_TIMER_H */
