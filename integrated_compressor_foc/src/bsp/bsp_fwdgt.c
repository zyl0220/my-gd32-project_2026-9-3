/*!
    \file    bsp_fwdgt.c
    \brief   独立看门狗板级支持包（BSP）—— FWDGT 复位保护实现

    使用固定预分频 DIV64：IRC32K(32kHz) / 64 = 500Hz，即每个计数 2ms。
    超时毫秒数换算成 12bit 重装值：reload = timeout_ms / 2，并夹取到 [1, 4095]。
    fwdgt_config() 内部已写入启动 key 并装载预分频/重装值，调用后看门狗即开始计数。

    \version 2026-06-22, V1.0, GD32C231C-EVAL 独立看门狗驱动
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

#include "bsp/bsp_fwdgt.h"
#include "gd32c2x1.h"

/*!
    \brief      按毫秒配置并启动看门狗
    \param[in]  timeout_ms: 期望的复位超时(毫秒)，超出 [2, 8190] 会被夹取
    \param[out] none
    \retval     BSP_FWDGT_OK=配置成功，BSP_FWDGT_FAIL=寄存器更新超时
*/
uint8_t bsp_fwdgt_init(uint32_t timeout_ms)
{
    uint32_t reload;

    /* 毫秒 -> 重装值(每 tick 2ms)，并夹取到 12bit 有效范围 [1, 4095] */
    if(timeout_ms > BSP_FWDGT_MAX_TIMEOUT_MS) {
        timeout_ms = BSP_FWDGT_MAX_TIMEOUT_MS;
    }
    reload = timeout_ms / BSP_FWDGT_MS_PER_TICK;
    if(reload == 0U) {
        reload = 1U;
    }

    /* DIV64 预分频；fwdgt_config 内部已启动计数器 */
    if(SUCCESS != fwdgt_config((uint16_t)reload, FWDGT_PSC_DIV64)) {
        return BSP_FWDGT_FAIL;
    }
    fwdgt_enable();

    return BSP_FWDGT_OK;
}

/*!
    \brief      喂狗（重装计数值），需在超时窗口内周期性调用
    \param[in]  none
    \param[out] none
    \retval     none
*/
void bsp_fwdgt_feed(void)
{
    fwdgt_counter_reload();
}

/*!
    \brief      判断本次上电是否由看门狗复位引起
    \param[in]  none
    \param[out] none
    \retval     1=本次复位由 FWDGT 引起(并已清除复位标志)，0=其它复位源
*/
uint8_t bsp_fwdgt_caused_reset(void)
{
    if(RESET != rcu_flag_get(RCU_FLAG_FWDGTRST)) {
        /* 清除全部复位标志，避免影响下次判断 */
        rcu_all_reset_flag_clear();
        return 1U;
    }
    return 0U;
}
