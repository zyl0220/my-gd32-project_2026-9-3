/*!
    \file    bsp_fwdgt.h
    \brief   独立看门狗板级支持包（BSP）—— FWDGT 复位保护

    硬件: 片上 FWDGT，时钟源固定为内部 IRC32K(32kHz)，无需任何外部资源
          预分频固定 DIV64 -> 计数频率 500Hz(2ms/tick)，理论超时范围约 2ms~8190ms
    实现参考: GD32C2x1_Firmware_Library_V1.3.0 / Examples/FWDGT/FWDGT_key

    \note 本模块按毫秒配置超时，并提供喂狗与“上电复位源判断”。
          喂狗点必须经过故障场景评审，不能只证明主循环局部仍在运行。
          初始化后必须在超时窗口内周期性调用 bsp_fwdgt_feed()，否则芯片复位。

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

#ifndef BSP_FWDGT_H
#define BSP_FWDGT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* DIV64 下计数频率 500Hz：每 tick 2ms，重装值 12bit -> 最大超时 4095*2≈8190ms */
#define BSP_FWDGT_MS_PER_TICK           2U
#define BSP_FWDGT_MAX_TIMEOUT_MS        8190U

#define BSP_FWDGT_OK                    0U
#define BSP_FWDGT_FAIL                  1U

/* 对外 API */
/* 按毫秒配置并启动看门狗；timeout_ms 超出范围会被夹取，返回 BSP_FWDGT_OK/FAIL */
uint8_t bsp_fwdgt_init(uint32_t timeout_ms);
/* 喂狗（重装计数值），需在超时窗口内周期性调用 */
void    bsp_fwdgt_feed(void);
/* 判断本次上电是否由看门狗复位引起；返回 1=是(并清标志) 0=否 */
uint8_t bsp_fwdgt_caused_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_FWDGT_H */
