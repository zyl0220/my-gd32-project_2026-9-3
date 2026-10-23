/*!
    \file    bsp_adc.c
    \brief   片上 ADC 板级支持包（BSP）—— 内部温度传感器与 VREFINT 采集实现

    使用 inserted（注入）序列扫描两个内部通道，软件触发、阻塞等待 EOIC/EOC：
        rank0 -> 温度传感器 (CH13)
        rank1 -> 内部参考电压 (CH14)
    温度换算沿用官方示例的经验公式（典型 VDDA=3.3V、斜率 2.52mV/°C、25°C 时 0.924V）。

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

#include "bsp/bsp_adc.h"
#include "systick.h"

/* 温度换算系数（来自官方 ADC_temperature_Vref 示例的经验值） */
#define ADC_TEMP_V25_V                  0.924f      /*!< 25°C 时温度传感器输出电压(V) */
#define ADC_TEMP_SLOPE_MV               2.52f       /*!< 温度斜率(mV/°C) */
#define ADC_TEMP_BASE_C                 25.0f       /*!< 基准温度(°C) */

/* 最近一次转换结果缓存：[0]=温度通道码值，[1]=VREF 通道码值 */
static uint16_t g_adc_raw[2];

/*!
    \brief      初始化 ADC（时钟 + inserted 双通道 + 内部温度/VREF 使能）
    \param[in]  none
    \param[out] none
    \retval     none
*/
void bsp_adc_init(void)
{
    /* 使能 ADC 时钟并选择 CK_SYS/10 作为 ADC 时钟（内部通道需要较慢采样时钟） */
    rcu_periph_clock_enable(RCU_ADC);
    rcu_adc_clock_config(RCU_ADCSRC_CKSYS, RCU_ADCCK_DIV10);

    /* 连续模式关、扫描模式开、数据右对齐 */
    adc_special_function_config(ADC_CONTINUOUS_MODE, DISABLE);
    adc_special_function_config(ADC_SCAN_MODE, ENABLE);
    adc_data_alignment_config(ADC_DATAALIGN_RIGHT);

    /* inserted 序列长度 2：rank0=温度，rank1=VREF，均取最长采样时间 160.5 周期 */
    adc_channel_length_config(ADC_INSERTED_CHANNEL, 2U);
    adc_inserted_channel_config(0U, BSP_ADC_TEMP_CHANNEL, ADC_SAMPLETIME_160POINT5);
    adc_inserted_channel_config(1U, BSP_ADC_VREF_CHANNEL, ADC_SAMPLETIME_160POINT5);

    /* inserted 触发源置 NONE 并使能外部触发使能位，使软件触发生效 */
    adc_external_trigger_source_config(ADC_INSERTED_CHANNEL, ADC_EXTTRIG_INSERTED_NONE);
    adc_external_trigger_config(ADC_INSERTED_CHANNEL, ENABLE);

    /* 使能片上温度传感器与内部参考电压通道 */
    adc_internal_channel_config(ADC_CHANNEL_INTERNAL_TEMPSENSOR, ENABLE);
    adc_internal_channel_config(ADC_CHANNEL_INTERNAL_VREFINT, ENABLE);

    /* 使能 ADC，等待稳定 */
    adc_enable();
    delay_1ms(1U);
}

/*!
    \brief      软件触发一次转换并等待完成，结果缓存在模块内部
    \param[in]  none
    \param[out] none
    \retval     none
*/
void bsp_adc_sample(void)
{
    /* 软件触发 inserted 序列 */
    adc_software_trigger_enable(ADC_INSERTED_CHANNEL);

    /* 等待 inserted 序列与整体转换完成标志 */
    while((adc_flag_get(ADC_FLAG_EOIC) == RESET) || (adc_flag_get(ADC_FLAG_EOC) == RESET)) {
    }

    /* 读出注入数据寄存器 IDATA0 / IDATA1 */
    g_adc_raw[0] = adc_inserted_data_read(ADC_INSERTED_CHANNEL_0);
    g_adc_raw[1] = adc_inserted_data_read(ADC_INSERTED_CHANNEL_1);

    /* 清转换完成标志 */
    adc_flag_clear(ADC_FLAG_EOIC);
    adc_flag_clear(ADC_FLAG_EOC);
}

/*!
    \brief      读取最近一次转换的温度通道原始码值
    \param[in]  none
    \param[out] none
    \retval     0~4095 的 12bit 码值
*/
uint16_t bsp_adc_get_temp_raw(void)
{
    return g_adc_raw[0];
}

/*!
    \brief      读取最近一次转换的 VREF 通道原始码值
    \param[in]  none
    \param[out] none
    \retval     0~4095 的 12bit 码值
*/
uint16_t bsp_adc_get_vref_raw(void)
{
    return g_adc_raw[1];
}

/*!
    \brief      把最近一次温度通道码值换算成摄氏度
    \param[in]  none
    \param[out] none
    \retval     温度（摄氏度，浮点）
*/
float bsp_adc_read_temperature(void)
{
    float v_sensor;

    /* 码值 -> 传感器电压(V) */
    v_sensor = ((float)g_adc_raw[0] * BSP_ADC_VDDA_MV / BSP_ADC_FULL_SCALE) / 1000.0f;

    /* (V25 - Vsensor)*1000 / 斜率(mV/°C) + 25°C */
    return (ADC_TEMP_V25_V - v_sensor) * 1000.0f / ADC_TEMP_SLOPE_MV + ADC_TEMP_BASE_C;
}

/*!
    \brief      把最近一次 VREF 通道码值换算成毫伏
    \param[in]  none
    \param[out] none
    \retval     内部参考电压（毫伏）
*/
uint16_t bsp_adc_read_vref_mv(void)
{
    return (uint16_t)((float)g_adc_raw[1] * BSP_ADC_VDDA_MV / BSP_ADC_FULL_SCALE);
}
