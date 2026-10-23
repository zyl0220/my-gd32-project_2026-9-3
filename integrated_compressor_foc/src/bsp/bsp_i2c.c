/*!
    \file    bsp_i2c.c
    \brief   通用 I2C 主机板级支持包（BSP）实现 —— 寄存器式读写

    时序（标志位序列）移植自本工程已验证的 bsp_i2c_eeprom.c，把“固定 EEPROM”泛化为
    “任意 8 位从地址 + 单字节寄存器地址”，并把跨页/reload 逻辑简化为“单次 <=255 字节、
    自动结束模式”，覆盖绝大多数传感器/RTC/扩展 IO 等寄存器式器件。

    GD32C2x1 I2C 采用 NBYTES 计数 + 自动结束(automatic end) 的传输模型：
      写：地址(W) -> 发 NBYTES 个字节(寄存器地址 + 数据) -> 到数自动产生 STOP
      读：地址(W) -> 发 1 字节(寄存器地址) -> 重复起始 -> 地址(R) -> 收 NBYTES 字节 -> 自动 STOP

    \version 2026-07-10, V1.0, GD32C231C8 通用 I2C 主机驱动
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

#include "bsp/bsp_i2c.h"
#include <stddef.h>     /* NULL */

/* 忙等某标志置位，超时返回非 0。宏封装是为了让每处等待都自带超时、不写死循环。 */
#define I2C_WAIT_FLAG(flag)                                            \
    do {                                                              \
        uint32_t _to = BSP_I2C_TIMEOUT;                               \
        while(RESET == i2c_flag_get(BSP_I2C_PERIPH, (flag))) {        \
            if(0U == --_to) { return BSP_I2C_FAIL; }                  \
        }                                                             \
    } while(0)

/* 忙等“总线空闲(I2CBSY=0)”，超时返回失败 */
static uint8_t i2c_wait_idle(void)
{
    uint32_t to = BSP_I2C_TIMEOUT;
    while(i2c_flag_get(BSP_I2C_PERIPH, I2C_FLAG_I2CBSY)) {
        if(0U == --to) {
            return BSP_I2C_FAIL;
        }
    }
    return BSP_I2C_OK;
}

/*!
    \brief      初始化 I2C0 引脚 + 控制器（400kHz）
    \param[in]  none
    \param[out] none
    \retval     none
*/
void bsp_i2c_init(void)
{
    /* 使能引脚与 I2C 时钟 */
    rcu_periph_clock_enable(BSP_I2C_SCL_RCU);
    rcu_periph_clock_enable(BSP_I2C_SDA_RCU);
    rcu_periph_clock_enable(BSP_I2C_RCU);

    /* SCL/SDA 均为“复用 + 开漏 + 上拉”：I2C 是线与总线，必须开漏(OD)，靠上拉回高。
       若外部已有强上拉电阻，这里的内部上拉可留可去；无外部上拉时内部上拉至少能让总线工作。 */
    gpio_af_set(BSP_I2C_SCL_PORT, BSP_I2C_SCL_AF, BSP_I2C_SCL_PIN);
    gpio_mode_set(BSP_I2C_SCL_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, BSP_I2C_SCL_PIN);
    gpio_output_options_set(BSP_I2C_SCL_PORT, GPIO_OTYPE_OD, GPIO_OSPEED_LEVEL_1, BSP_I2C_SCL_PIN);

    gpio_af_set(BSP_I2C_SDA_PORT, BSP_I2C_SDA_AF, BSP_I2C_SDA_PIN);
    gpio_mode_set(BSP_I2C_SDA_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, BSP_I2C_SDA_PIN);
    gpio_output_options_set(BSP_I2C_SDA_PORT, GPIO_OTYPE_OD, GPIO_OSPEED_LEVEL_1, BSP_I2C_SDA_PIN);

    /* 时序参数与官方 400kHz 一致（与 bsp_i2c_eeprom 同一套时钟推算值） */
    i2c_timing_config(BSP_I2C_PERIPH, 0, 0x4, 0);
    i2c_master_clock_config(BSP_I2C_PERIPH, 0x1C, 0x55);
    i2c_enable(BSP_I2C_PERIPH);
}

/*!
    \brief      向从设备 reg 起始位置写入 len 字节
    \param[in]  dev_addr: 从设备 8 位地址（7 位地址左移 1 位）
    \param[in]  reg: 起始寄存器地址（单字节）
    \param[in]  data: 待写数据
    \param[in]  len: 字节数（1~255）
    \param[out] none
    \retval     BSP_I2C_OK / BSP_I2C_FAIL
*/
uint8_t bsp_i2c_mem_write(uint8_t dev_addr, uint8_t reg, const uint8_t *data, uint16_t len)
{
    uint16_t i;

    if((NULL == data) || (0U == len) || (len > 255U)) {
        return BSP_I2C_FAIL;
    }
    if(BSP_I2C_OK != i2c_wait_idle()) {
        return BSP_I2C_FAIL;
    }

    /* 传输方向=写；NBYTES = 寄存器地址(1) + 数据(len)；到数后自动 STOP */
    i2c_master_addressing(BSP_I2C_PERIPH, dev_addr, I2C_MASTER_TRANSMIT);
    i2c_transfer_byte_number_config(BSP_I2C_PERIPH, (uint8_t)(len + 1U));
    i2c_automatic_end_enable(BSP_I2C_PERIPH);

    /* 起始条件（含发送从地址） */
    i2c_start_on_bus(BSP_I2C_PERIPH);

    /* 先发寄存器地址：等发送缓冲空(TBE) */
    I2C_WAIT_FLAG(I2C_FLAG_TBE);
    i2c_data_transmit(BSP_I2C_PERIPH, reg);

    /* 逐字节发数据：每字节等 TI(可继续发) */
    for(i = 0U; i < len; i++) {
        I2C_WAIT_FLAG(I2C_FLAG_TI);
        i2c_data_transmit(BSP_I2C_PERIPH, data[i]);
    }

    /* 等自动产生的 STOP 被检测到，并清标志，一次事务完成 */
    I2C_WAIT_FLAG(I2C_FLAG_STPDET);
    i2c_flag_clear(BSP_I2C_PERIPH, I2C_FLAG_STPDET);
    return BSP_I2C_OK;
}

/*!
    \brief      从从设备 reg 起始位置读出 len 字节
    \param[in]  dev_addr: 从设备 8 位地址
    \param[in]  reg: 起始寄存器地址（单字节）
    \param[in]  len: 字节数（1~255）
    \param[out] data: 读出数据写入此处
    \retval     BSP_I2C_OK / BSP_I2C_FAIL
*/
uint8_t bsp_i2c_mem_read(uint8_t dev_addr, uint8_t reg, uint8_t *data, uint16_t len)
{
    uint16_t i;

    if((NULL == data) || (0U == len) || (len > 255U)) {
        return BSP_I2C_FAIL;
    }
    if(BSP_I2C_OK != i2c_wait_idle()) {
        return BSP_I2C_FAIL;
    }

    /* 第一阶段：写方向发出“寄存器地址”，但先不发 STOP（automatic end 关），以便随后重复起始。
       NBYTES=1 表示只发 1 个字节(寄存器地址)。 */
    i2c_master_addressing(BSP_I2C_PERIPH, dev_addr, I2C_MASTER_TRANSMIT);
    i2c_transfer_byte_number_config(BSP_I2C_PERIPH, 1U);
    i2c_automatic_end_disable(BSP_I2C_PERIPH);
    i2c_start_on_bus(BSP_I2C_PERIPH);

    I2C_WAIT_FLAG(I2C_FLAG_TBE);
    i2c_data_transmit(BSP_I2C_PERIPH, reg);
    /* 等“传输完成(TC)”：寄存器地址已发完，可以发重复起始切到读方向 */
    I2C_WAIT_FLAG(I2C_FLAG_TC);

    /* 第二阶段：读方向，NBYTES=len，收满自动 STOP */
    i2c_master_addressing(BSP_I2C_PERIPH, dev_addr, I2C_MASTER_RECEIVE);
    i2c_transfer_byte_number_config(BSP_I2C_PERIPH, (uint8_t)len);
    i2c_automatic_end_enable(BSP_I2C_PERIPH);
    i2c_start_on_bus(BSP_I2C_PERIPH);   /* 重复起始 */

    /* 逐字节读：每字节等接收缓冲非空(RBNE) */
    for(i = 0U; i < len; i++) {
        I2C_WAIT_FLAG(I2C_FLAG_RBNE);
        data[i] = i2c_data_receive(BSP_I2C_PERIPH);
    }

    /* 等自动 STOP 并清标志 */
    I2C_WAIT_FLAG(I2C_FLAG_STPDET);
    i2c_flag_clear(BSP_I2C_PERIPH, I2C_FLAG_STPDET);
    return BSP_I2C_OK;
}

/*!
    \brief      写单个寄存器
    \param[in]  dev_addr: 从设备 8 位地址
    \param[in]  reg: 寄存器地址
    \param[in]  value: 写入值
    \param[out] none
    \retval     BSP_I2C_OK / BSP_I2C_FAIL
*/
uint8_t bsp_i2c_write_byte(uint8_t dev_addr, uint8_t reg, uint8_t value)
{
    return bsp_i2c_mem_write(dev_addr, reg, &value, 1U);
}

/*!
    \brief      读单个寄存器
    \param[in]  dev_addr: 从设备 8 位地址
    \param[in]  reg: 寄存器地址
    \param[out] value: 读出值写入此处
    \retval     BSP_I2C_OK / BSP_I2C_FAIL
*/
uint8_t bsp_i2c_read_byte(uint8_t dev_addr, uint8_t reg, uint8_t *value)
{
    if(NULL == value) {
        return BSP_I2C_FAIL;
    }
    return bsp_i2c_mem_read(dev_addr, reg, value, 1U);
}

/*!
    \brief      探测器件是否在位（发地址+0 字节，收到应答即在位）
    \param[in]  dev_addr: 从设备 8 位地址
    \param[out] none
    \retval     BSP_I2C_OK=在位, BSP_I2C_FAIL=无应答/超时
*/
uint8_t bsp_i2c_probe(uint8_t dev_addr)
{
    uint32_t to;

    if(BSP_I2C_OK != i2c_wait_idle()) {
        return BSP_I2C_FAIL;
    }

    /* 只发从地址、NBYTES=0、自动结束：从设备应答则正常产生 STOP，否则回 NACK */
    i2c_master_addressing(BSP_I2C_PERIPH, dev_addr, I2C_MASTER_TRANSMIT);
    i2c_transfer_byte_number_config(BSP_I2C_PERIPH, 0U);
    i2c_automatic_end_enable(BSP_I2C_PERIPH);
    i2c_start_on_bus(BSP_I2C_PERIPH);

    /* 等到 STOP(有应答) 或 NACK(无应答) 二者之一 */
    to = BSP_I2C_TIMEOUT;
    while((RESET == i2c_flag_get(BSP_I2C_PERIPH, I2C_FLAG_STPDET)) &&
          (RESET == i2c_flag_get(BSP_I2C_PERIPH, I2C_FLAG_NACK))) {
        if(0U == --to) {
            return BSP_I2C_FAIL;
        }
    }

    if(RESET != i2c_flag_get(BSP_I2C_PERIPH, I2C_FLAG_NACK)) {
        /* 无应答：清掉 NACK 与 STOP 标志后判失败 */
        i2c_flag_clear(BSP_I2C_PERIPH, I2C_FLAG_NACK);
        i2c_flag_clear(BSP_I2C_PERIPH, I2C_FLAG_STPDET);
        return BSP_I2C_FAIL;
    }

    i2c_flag_clear(BSP_I2C_PERIPH, I2C_FLAG_STPDET);
    return BSP_I2C_OK;
}
