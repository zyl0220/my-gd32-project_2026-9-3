/*!
    \file    bsp_pwm.c
    \brief   定时器 PWM 板级支持包（BSP）—— TIMER2 通道 0 PWM 输出实现

    TIMER2 边沿对齐、向上计数，CH0 工作在 PWM0 模式：
        计数 < CHxCV 时输出有效电平，>= CHxCV 时翻转，故 CHxCV 即占空比。
    运行期通过 timer_channel_output_pulse_value_config() 改写 CHxCV 即可动态调光。

    \warning 输出引脚 PB4 在本板上同时是板载 LED4。使用本 PWM 模块即把 PB4 接管为
             PWM 输出，此时不要再对 BSP_LED4 调用 bsp_led_xx()（二者择一）。

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

#include "bsp/bsp_pwm.h"

/* 呼吸灯步进器的运行时状态：当前占空比(千分比)与方向(+1/-1) */
#define BSP_PWM_BREATH_STEP             20U         /*!< 每步占空比增量(千分比) */
static uint16_t g_breath_duty = 0U;
static int8_t   g_breath_dir  = 1;

/*!
    \brief      配置 PWM 输出引脚为复用功能
    \param[in]  none
    \param[out] none
    \retval     none
*/
static void bsp_pwm_gpio_config(void)
{
    rcu_periph_clock_enable(BSP_PWM_GPIO_CLK);

    gpio_mode_set(BSP_PWM_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, BSP_PWM_GPIO_PIN);
    gpio_output_options_set(BSP_PWM_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_LEVEL_1, BSP_PWM_GPIO_PIN);
    gpio_af_set(BSP_PWM_GPIO_PORT, BSP_PWM_GPIO_AF, BSP_PWM_GPIO_PIN);
}

/*!
    \brief      初始化 PWM（GPIO + TIMER2 + CH0），初始占空比 0%
    \param[in]  none
    \param[out] none
    \retval     none
*/
void bsp_pwm_init(void)
{
    timer_oc_parameter_struct timer_ocinitpara;
    timer_parameter_struct    timer_initpara;

    bsp_pwm_gpio_config();

    rcu_periph_clock_enable(BSP_PWM_TIMER_CLK);
    timer_deinit(BSP_PWM_TIMER);

    /* 时基：边沿对齐、向上计数，频率见头文件宏注释 */
    timer_struct_para_init(&timer_initpara);
    timer_initpara.prescaler        = BSP_PWM_PRESCALER;
    timer_initpara.alignedmode      = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection = TIMER_COUNTER_UP;
    timer_initpara.period           = BSP_PWM_PERIOD;
    timer_initpara.clockdivision    = TIMER_CKDIV_DIV1;
    timer_init(BSP_PWM_TIMER, &timer_initpara);

    /* 通道输出配置：使能主输出、互补输出关、高电平有效 */
    timer_channel_output_struct_para_init(&timer_ocinitpara);
    timer_ocinitpara.outputstate  = TIMER_CCX_ENABLE;
    timer_ocinitpara.outputnstate = TIMER_CCXN_DISABLE;
    timer_ocinitpara.ocpolarity   = TIMER_OC_POLARITY_HIGH;
    timer_ocinitpara.ocnpolarity  = TIMER_OCN_POLARITY_HIGH;
    timer_ocinitpara.ocidlestate  = TIMER_OC_IDLE_STATE_LOW;
    timer_ocinitpara.ocnidlestate = TIMER_OCN_IDLE_STATE_LOW;
    timer_channel_output_config(BSP_PWM_TIMER, BSP_PWM_CHANNEL, &timer_ocinitpara);

    /* PWM0 模式，初始占空比 0 */
    timer_channel_output_pulse_value_config(BSP_PWM_TIMER, BSP_PWM_CHANNEL, 0U);
    timer_channel_output_mode_config(BSP_PWM_TIMER, BSP_PWM_CHANNEL, TIMER_OC_MODE_PWM0);

    timer_enable(BSP_PWM_TIMER);
}

/*!
    \brief      设置占空比，按千分比 0~1000
    \param[in]  permille: 占空比千分比，超过 1000 按 1000 处理
    \param[out] none
    \retval     none
*/
void bsp_pwm_set_duty_permille(uint16_t permille)
{
    uint32_t pulse;

    if(permille > BSP_PWM_DUTY_FULL) {
        permille = BSP_PWM_DUTY_FULL;
    }
    /* 千分比 -> CHxCV：pulse = permille * (PERIOD+1) / 1000 */
    pulse = ((uint32_t)permille * (BSP_PWM_PERIOD + 1U)) / BSP_PWM_DUTY_FULL;
    timer_channel_output_pulse_value_config(BSP_PWM_TIMER, BSP_PWM_CHANNEL, pulse);
}

/*!
    \brief      设置占空比，按百分比 0~100
    \param[in]  percent: 占空比百分比，超过 100 按 100 处理
    \param[out] none
    \retval     none
*/
void bsp_pwm_set_duty_percent(uint8_t percent)
{
    if(percent > 100U) {
        percent = 100U;
    }
    bsp_pwm_set_duty_permille((uint16_t)percent * 10U);
}

/*!
    \brief      呼吸灯步进器：每调用一次占空比走一小步，三角波往返
    \param[in]  none
    \param[out] none
    \retval     none
*/
void bsp_pwm_breath_step(void)
{
    if(g_breath_dir > 0) {
        if(g_breath_duty >= (BSP_PWM_DUTY_FULL - BSP_PWM_BREATH_STEP)) {
            g_breath_duty = BSP_PWM_DUTY_FULL;
            g_breath_dir  = -1;                 /* 到顶，转为减小 */
        } else {
            g_breath_duty += BSP_PWM_BREATH_STEP;
        }
    } else {
        if(g_breath_duty <= BSP_PWM_BREATH_STEP) {
            g_breath_duty = 0U;
            g_breath_dir  = 1;                  /* 到底，转为增大 */
        } else {
            g_breath_duty -= BSP_PWM_BREATH_STEP;
        }
    }
    bsp_pwm_set_duty_permille(g_breath_duty);
}
