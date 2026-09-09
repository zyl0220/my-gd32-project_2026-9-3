/*!
    \file    bsp_spi.h
    \brief   通用 SPI 主机板级支持包（BSP）—— 全双工、8 位、软件片选，供随手调用

    硬件: SPI0 主机
          SCK  = PB3 (AF0)
          MISO = PA6 (AF0)
          MOSI = PB5 (AF0)
          CS   = PA5 (普通 GPIO 输出, 软件控制, 低有效)
    实现参考: GD32C2x1_Firmware_Library_V1.3.0 / Examples/SPI/SPI_master_slave_fullduplex_polling

    ============================ 设计说明 / 为什么这样配 ============================
    1) 软件片选(SPI_NSS_SOFT) + 独立 GPIO 做 CS：
       主机模式下用硬件 NSS 会牵扯 NSS 引脚电平与“多主机冲突检测(MODF)”，容易误触发
       主机模式故障。这里把 NSS 交给软件(SPI_NSS_SOFT)，片选另用一个普通 GPIO(PA4)
       手动拉低/拉高，最简单可靠，也方便一条总线挂多个从设备（每个从设备各给一个 CS）。

    2) GD32C2x1 的 SPI 带 FIFO，必须显式设成“按字节访问”：
       复位默认是按半字(16bit)访问 FIFO。做 8 位传输时若不调用
       spi_fifo_access_size_config(SPIx, SPI_BYTE_ACCESS)，写入/读出会按 16bit 对齐，
       导致每次实际发 2 字节、收发错位。bsp_spi_init() 里已处理，务必保留。

    3) 主机软件 NSS 必须把内部 NSS 拉高(spi_nss_internal_high)：
       软件 NSS 模式下，内部 NSS 若为低会被判为“别的主机在占用总线”，触发 MODF 而使
       SPI 自动退出主机模式、停止工作。init 里调用 spi_nss_internal_high() 规避。

    4) 时钟极性/相位默认 Mode 0 (CPOL=0, CPHA=0)，速率默认 = APB/32。
       换从设备时序：改 BSP_SPI_CK_MODE、BSP_SPI_PRESCALE 即可，其余不用动。
    ================================================================================

    \note 换 SPI 实例 / 换引脚 / 换 CS 只改下方宏，bsp_spi.c 无需改动。
          MISO 在从设备不驱动总线时为高阻，本模块给 MISO 引脚配了上拉，读“空总线”得 0xFF。

    使用示例（读一个 SPI Flash 的 JEDEC ID 命令 0x9F 为例）:
        uint8_t id[3];
        bsp_spi_init();
        bsp_spi_cs_low();                 // 选中从设备
        (void)bsp_spi_transfer_byte(0x9F);// 发命令，忽略回读
        id[0] = bsp_spi_transfer_byte(0xFF); // 发 dummy 收数据
        id[1] = bsp_spi_transfer_byte(0xFF);
        id[2] = bsp_spi_transfer_byte(0xFF);
        bsp_spi_cs_high();                // 释放从设备

    \version 2026-07-10, V1.0, GD32C231C8 通用 SPI 主机驱动
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

#ifndef BSP_SPI_H
#define BSP_SPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "gd32c2x1.h"
#include <stdint.h>

/* ---- 控制器与引脚资源映射（换 SPI 实例 / 换引脚改这里）----
   注意 SCK/MISO/MOSI 三根信号线各自的端口/AF 号要对上数据手册复用功能表。 */
#define BSP_SPI_PERIPH         SPI0
#define BSP_SPI_RCU            RCU_SPI0

#define BSP_SPI_SCK_PORT       GPIOB
#define BSP_SPI_SCK_PIN        GPIO_PIN_3
#define BSP_SPI_SCK_AF         GPIO_AF_0
#define BSP_SPI_SCK_RCU        RCU_GPIOB

#define BSP_SPI_MISO_PORT      GPIOA
#define BSP_SPI_MISO_PIN       GPIO_PIN_6
#define BSP_SPI_MISO_AF        GPIO_AF_0
#define BSP_SPI_MISO_RCU       RCU_GPIOA

#define BSP_SPI_MOSI_PORT      GPIOB
#define BSP_SPI_MOSI_PIN       GPIO_PIN_5
#define BSP_SPI_MOSI_AF        GPIO_AF_0
#define BSP_SPI_MOSI_RCU       RCU_GPIOB

/* 片选(CS)：普通 GPIO 输出，软件控制，低有效。挂多个从设备时可复制一组另配 CS。
   注意：本板 PA4 已被 bsp_key 的 USER 键占用，故 CS 用空闲的 PA5，避免引脚冲突。 */
#define BSP_SPI_CS_PORT        GPIOA
#define BSP_SPI_CS_PIN         GPIO_PIN_5
#define BSP_SPI_CS_RCU         RCU_GPIOA

/* 时序参数：
   BSP_SPI_CK_MODE  时钟极性+相位。SPI_CK_PL_LOW_PH_1EDGE = Mode0(最常见)。
                    其余可选: *_HIGH_PH_1EDGE=Mode2, *_LOW_PH_2EDGE=Mode1, *_HIGH_PH_2EDGE=Mode3。
   BSP_SPI_PRESCALE SCK = f_APB / 分频。默认 32 分频；从设备能跑更快可减小(如 SPI_PSC_8)。 */
#define BSP_SPI_CK_MODE        SPI_CK_PL_LOW_PH_1EDGE
#define BSP_SPI_PRESCALE       SPI_PSC_32

/* 收发超时的空转上限（防止从设备不应答时死等）。数值为忙等循环次数，非精确时间。 */
#define BSP_SPI_TIMEOUT        ((uint32_t)0x00100000U)

/* CS 有效/无效电平（低有效：选中=低，释放=高） */
#define BSP_SPI_CS_ACTIVE      RESET
#define BSP_SPI_CS_IDLE        SET

/* ---- 对外 API ---- */
/* 初始化 SPI0 主机(引脚 + 控制器 + FIFO 字节访问 + 软 NSS)，并把 CS 置为释放态，main 早期调用一次 */
void    bsp_spi_init(void);
/* 拉低 CS，选中从设备（一次事务开始时调用） */
void    bsp_spi_cs_low(void);
/* 拉高 CS，释放从设备（一次事务结束时调用） */
void    bsp_spi_cs_high(void);
/* 全双工交换一个字节：发出 tx 的同时把 MOSI/MISO 上收到的字节返回（SPI 收发同步进行） */
uint8_t bsp_spi_transfer_byte(uint8_t tx);
/* 全双工交换一段缓冲区：逐字节调用 transfer_byte。
   tx 为 NULL 时发 0xFF 占位（纯读）；rx 为 NULL 时丢弃回读（纯写）；两者都给则边发边收。 */
void    bsp_spi_transfer(const uint8_t *tx, uint8_t *rx, uint16_t len);
/* 纯写 len 字节（回读丢弃），等价于 bsp_spi_transfer(buf, NULL, len) */
void    bsp_spi_write(const uint8_t *buffer, uint16_t len);
/* 纯读 len 字节（发 0xFF 占位），等价于 bsp_spi_transfer(NULL, buf, len) */
void    bsp_spi_read(uint8_t *buffer, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* BSP_SPI_H */
