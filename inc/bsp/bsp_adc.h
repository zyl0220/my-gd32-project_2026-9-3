/*!
    \file    bsp_adc.h
    \brief   片上 ADC 板级支持包（BSP）—— 采集内部温度传感器与内部参考电压 VREFINT

    硬件: 仅使用芯片内部模拟通道，无需任何外部接线
          内部温度传感器 -> ADC inserted CH13
          内部参考电压   -> ADC inserted CH14
          ADC 时钟       -> CK_SYS / 10
    实现参考: GD32C2x1_Firmware_Library_V1.3.0 / Examples/ADC/ADC_temperature_Vref

    \note 本模块为项目自有板级代码，自带换算公式（°C / mV），适合上电自检；
          CMake 会自动编译 src/bsp/ 下的实现。

    \version 2026-06-22, V1.0, GD32C231C-EVAL 片上 ADC 驱动
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

#ifndef BSP_ADC_H
#define BSP_ADC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "gd32c2x1.h"

/* 内部通道映射（GD32C2x1：温度传感器=CH13，VREFINT=CH14；换芯片前先查数据手册确认） */
#define BSP_ADC_TEMP_CHANNEL            ADC_CHANNEL_13
#define BSP_ADC_VREF_CHANNEL            ADC_CHANNEL_14

/* ADC 满量程与供电参考电压（12bit -> 4095；VDDA 默认按 3.3V 计算电压） */
#define BSP_ADC_FULL_SCALE             4095.0f
#define BSP_ADC_VDDA_MV                3300.0f

/* 对外 API */
/* 初始化 ADC（时钟 + inserted 双通道 + 内部温度/VREF 使能） */
void     bsp_adc_init(void);
/* 软件触发一次转换并等待完成，结果缓存在模块内部 */
void     bsp_adc_sample(void);
/* 读取最近一次转换的温度通道原始码值（0~4095） */
uint16_t bsp_adc_get_temp_raw(void);
/* 读取最近一次转换的 VREF 通道原始码值（0~4095） */
uint16_t bsp_adc_get_vref_raw(void);
/* 把最近一次温度通道码值换算成摄氏度 */
float    bsp_adc_read_temperature(void);
/* 把最近一次 VREF 通道码值换算成毫伏 */
uint16_t bsp_adc_read_vref_mv(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_ADC_H */
