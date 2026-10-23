/*!
    \file    bsp_i2c.h
    \brief   通用 I2C 主机板级支持包（BSP）—— 面向“寄存器式”从设备的读写

    硬件: I2C0, SCL=PB8 (AF6) / SDA=PB9 (AF6), 400kHz, 开漏 + 上拉
    实现参考: GD32C2x1_Firmware_Library_V1.3.0 / Examples/I2C（标志位时序移植自本工程
              已验证的 bsp_i2c_eeprom.c，仅泛化为“任意从地址 + 单字节寄存器地址”）

    ============================== 和 EEPROM 模块的分工 ==============================
    - bsp_i2c_eeprom 用的是 I2C1 (PA11/PA12)，专门伺候 AT24C02。
    - 本模块用另一条独立总线 I2C0 (PB8/PB9)，给你挂各种“寄存器式”I2C 器件
      （传感器、RTC、扩展 IO、EEPROM 等），两条总线互不影响、引脚不冲突。
    ================================================================================

    约定与限制：
    - 从地址用“8 位形式”（即 7 位地址左移 1 位、最低位留 0），例如器件 7 位地址 0x50，
      这里传 0xA0。读/写方向由库自动处理，你只管传这个 8 位基地址。
    - “寄存器地址”按单字节(8 位)处理，覆盖绝大多数传感器/RTC/EEPROM；
      需要 16 位寄存器地址的器件本模块暂不支持（可另行扩展）。
    - 单次传输长度 <= 255 字节（采用自动结束模式，不做 reload 分段）。
    - 所有 API 带超时，从设备不应答不会死等，失败返回 BSP_I2C_FAIL。

    使用示例（读某传感器 WHO_AM_I 寄存器 0x0F，器件 8 位地址 0xD0）:
        uint8_t who = 0;
        bsp_i2c_init();
        if(BSP_I2C_OK == bsp_i2c_read_byte(0xD0, 0x0F, &who)) { ... }

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

#ifndef BSP_I2C_H
#define BSP_I2C_H

#ifdef __cplusplus
extern "C" {
#endif

#include "gd32c2x1.h"
#include <stdint.h>

/* ---- 硬件资源映射（换 I2C 实例 / 换引脚改这里；I2C 信号在本芯片为 AF6）---- */
#define BSP_I2C_PERIPH         I2C0
#define BSP_I2C_RCU            RCU_I2C0

#define BSP_I2C_SCL_PORT       GPIOB
#define BSP_I2C_SCL_PIN        GPIO_PIN_8
#define BSP_I2C_SCL_RCU        RCU_GPIOB
#define BSP_I2C_SCL_AF         GPIO_AF_6

#define BSP_I2C_SDA_PORT       GPIOB
#define BSP_I2C_SDA_PIN        GPIO_PIN_9
#define BSP_I2C_SDA_RCU        RCU_GPIOB
#define BSP_I2C_SDA_AF         GPIO_AF_6

/* 忙等超时上限（循环次数，非精确时间），从设备不应答时用于兜底退出 */
#define BSP_I2C_TIMEOUT        ((uint32_t)0x00080000U)

/* 返回码 */
#define BSP_I2C_OK             0U
#define BSP_I2C_FAIL           1U

/* ---- 对外 API ---- */
/* 初始化 I2C0 引脚 + 控制器（400kHz），main 早期调用一次 */
void    bsp_i2c_init(void);

/* 向从设备的 reg 寄存器起始位置写入 len 字节，返回 BSP_I2C_OK/BSP_I2C_FAIL */
uint8_t bsp_i2c_mem_write(uint8_t dev_addr, uint8_t reg, const uint8_t *data, uint16_t len);
/* 从从设备的 reg 寄存器起始位置读出 len 字节，返回 BSP_I2C_OK/BSP_I2C_FAIL */
uint8_t bsp_i2c_mem_read(uint8_t dev_addr, uint8_t reg, uint8_t *data, uint16_t len);

/* 写单个寄存器（一字节），返回 BSP_I2C_OK/BSP_I2C_FAIL */
uint8_t bsp_i2c_write_byte(uint8_t dev_addr, uint8_t reg, uint8_t value);
/* 读单个寄存器（一字节），结果写入 *value，返回 BSP_I2C_OK/BSP_I2C_FAIL */
uint8_t bsp_i2c_read_byte(uint8_t dev_addr, uint8_t reg, uint8_t *value);

/* 探测器件是否在位（只发地址看是否应答），在位返回 BSP_I2C_OK */
uint8_t bsp_i2c_probe(uint8_t dev_addr);

#ifdef __cplusplus
}
#endif

#endif /* BSP_I2C_H */
