/*!
    \file    bsp_pwm.h
    \brief   定时器 PWM 板级支持包（BSP）—— TIMER2 通道 0 输出可调占空比 PWM

    硬件: TIMER2_CH0 -> PB4 (AF1)
    实现参考: GD32C2x1_Firmware_Library_V1.3.0 / Examples/TIMER/TIMER2 pwmout

    \warning PB4 在本板上同时是板载 LED4。使用本 PWM 模块即把 PB4 接管为 PWM 输出，
             此时不要再对 BSP_LED4 调用 bsp_led_xx()。反过来，这也让 PWM 可直接驱动
             LED4 做“呼吸灯”，无需任何外部接线。

    \note 频率/占空比由下方宏与 API 决定。定时器计数频率默认按 CK_TIMER2 = 48MHz 推算
          （与官方示例一致）；若系统时钟不同，请改 BSP_PWM_PRESCALER 使计数频率=1MHz。

    \version 2026-06-22, V1.0, GD32C231C-EVAL 定时器 PWM 驱动
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

#ifndef BSP_PWM_H
#define BSP_PWM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "gd32c2x1.h"

/* 引脚与定时器资源映射（换板/换通道改这里） */
#define BSP_PWM_TIMER                   TIMER2
#define BSP_PWM_TIMER_CLK               RCU_TIMER2
#define BSP_PWM_CHANNEL                 TIMER_CH_0
#define BSP_PWM_GPIO_PORT               GPIOB       /* TIMER2_CH0 -> PB4 */
#define BSP_PWM_GPIO_PIN                GPIO_PIN_4
#define BSP_PWM_GPIO_CLK                RCU_GPIOB
#define BSP_PWM_GPIO_AF                 GPIO_AF_1

/* 计数频率 = CK_TIMER2 / (BSP_PWM_PRESCALER+1)。默认 48MHz/(47+1)=1MHz */
#define BSP_PWM_PRESCALER               47U
/* PWM 周期 = (BSP_PWM_PERIOD+1) 个计数 = 1000 个 -> 1MHz/1000 = 1kHz PWM */
#define BSP_PWM_PERIOD                  999U
/* 占空比满刻度（与 PERIOD+1 相同，便于 0~1000 的千分比设置） */
#define BSP_PWM_DUTY_FULL               1000U

/* 对外 API */
/* 初始化 PWM（GPIO + TIMER2 + CH0），初始占空比 0% */
void bsp_pwm_init(void);
/* 设置占空比，按百分比 0~100 */
void bsp_pwm_set_duty_percent(uint8_t percent);
/* 设置占空比，按千分比 0~1000（分辨率更高） */
void bsp_pwm_set_duty_permille(uint16_t permille);
/* 呼吸灯步进器：每调用一次占空比走一小步，三角波往返，需周期性调用 */
void bsp_pwm_breath_step(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_PWM_H */
