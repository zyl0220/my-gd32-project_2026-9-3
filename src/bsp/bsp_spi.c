/*!
    \file    bsp_spi.c
    \brief   通用 SPI 主机板级支持包（BSP）实现 —— 全双工、8 位、软件片选

    引脚/控制器配置与收发时序沿用官方例程 GD32C2x1_Firmware_Library_V1.3.0 /
    Examples/SPI/SPI_master_slave_fullduplex_polling 的做法，仅做模块化封装。
    GD32C2x1 SPI 的两处关键点（FIFO 字节访问、主机软 NSS 内部拉高）在 bsp_spi.h
    的设计说明里有详细解释，本文件的对应代码行也加了行内注释。

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

#include "bsp/bsp_spi.h"
#include <stddef.h>     /* NULL */

/* 纯读事务里，向从设备发出的占位字节。多数 SPI 从设备约定读时主机发 0xFF。 */
#define BSP_SPI_DUMMY_BYTE     0xFFU

/*!
    \brief      配置 SPI 三根信号线的 GPIO（SCK/MISO/MOSI）
    \param[in]  none
    \param[out] none
    \retval     none
*/
static void bsp_spi_gpio_config(void)
{
    /* 分别使能三根信号线所在端口时钟（可能不在同一个 GPIO 端口，故逐个使能） */
    rcu_periph_clock_enable(BSP_SPI_SCK_RCU);
    rcu_periph_clock_enable(BSP_SPI_MISO_RCU);
    rcu_periph_clock_enable(BSP_SPI_MOSI_RCU);

    /* SCK：复用推挽输出。SPI 时钟由主机产生，必须推挽以保证上升/下降沿陡峭。 */
    gpio_af_set(BSP_SPI_SCK_PORT, BSP_SPI_SCK_AF, BSP_SPI_SCK_PIN);
    gpio_mode_set(BSP_SPI_SCK_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, BSP_SPI_SCK_PIN);
    gpio_output_options_set(BSP_SPI_SCK_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_LEVEL_1, BSP_SPI_SCK_PIN);

    /* MOSI：复用推挽输出，主机往从设备发数据。 */
    gpio_af_set(BSP_SPI_MOSI_PORT, BSP_SPI_MOSI_AF, BSP_SPI_MOSI_PIN);
    gpio_mode_set(BSP_SPI_MOSI_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, BSP_SPI_MOSI_PIN);
    gpio_output_options_set(BSP_SPI_MOSI_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_LEVEL_1, BSP_SPI_MOSI_PIN);

    /* MISO：复用功能、加上拉。从设备不选中时 MISO 处于高阻，上拉可让空总线读到稳定的 0xFF。 */
    gpio_af_set(BSP_SPI_MISO_PORT, BSP_SPI_MISO_AF, BSP_SPI_MISO_PIN);
    gpio_mode_set(BSP_SPI_MISO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, BSP_SPI_MISO_PIN);
    gpio_output_options_set(BSP_SPI_MISO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_LEVEL_1, BSP_SPI_MISO_PIN);
}

/*!
    \brief      配置 CS 片选 GPIO（普通推挽输出，初始为释放态=高）
    \param[in]  none
    \param[out] none
    \retval     none
*/
static void bsp_spi_cs_config(void)
{
    rcu_periph_clock_enable(BSP_SPI_CS_RCU);
    gpio_mode_set(BSP_SPI_CS_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, BSP_SPI_CS_PIN);
    gpio_output_options_set(BSP_SPI_CS_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_LEVEL_1, BSP_SPI_CS_PIN);
    /* 上电先释放从设备，避免上电瞬间误选中 */
    gpio_bit_write(BSP_SPI_CS_PORT, BSP_SPI_CS_PIN, BSP_SPI_CS_IDLE);
}

/*!
    \brief      初始化 SPI0 主机（引脚 + 控制器 + FIFO 字节访问 + 软 NSS），CS 置释放态
    \param[in]  none
    \param[out] none
    \retval     none
*/
void bsp_spi_init(void)
{
    spi_parameter_struct spi_init_struct;

    /* 1) 先配好三根信号线与片选 GPIO */
    bsp_spi_gpio_config();
    bsp_spi_cs_config();

    /* 2) 使能 SPI 控制器时钟 */
    rcu_periph_clock_enable(BSP_SPI_RCU);

    /* 3) 复位 SPI 并填充参数结构体默认值，再逐项覆盖为主机全双工 8bit */
    spi_i2s_deinit(BSP_SPI_PERIPH);
    spi_struct_para_init(&spi_init_struct);
    spi_init_struct.trans_mode           = SPI_TRANSMODE_FULLDUPLEX;  /* 全双工：发的同时收 */
    spi_init_struct.device_mode          = SPI_MASTER;               /* 本机为主机，产生 SCK */
    spi_init_struct.frame_size           = SPI_FRAMESIZE_8BIT;       /* 8 位数据帧 */
    spi_init_struct.clock_polarity_phase = BSP_SPI_CK_MODE;          /* 时钟极性/相位，默认 Mode0 */
    spi_init_struct.nss                  = SPI_NSS_SOFT;             /* 软件 NSS，片选另用 GPIO */
    spi_init_struct.prescale             = BSP_SPI_PRESCALE;         /* SCK 分频 */
    spi_init_struct.endian               = SPI_ENDIAN_MSB;          /* 高位先发，SPI 惯例 */
    spi_init(BSP_SPI_PERIPH, &spi_init_struct);

    /* 4) 关键：把 FIFO 访问宽度设为“按字节”。
          GD32C2x1 SPI 带 FIFO，复位默认按 16bit 访问；不改这里做 8bit 收发会错位。 */
    spi_fifo_access_size_config(BSP_SPI_PERIPH, SPI_BYTE_ACCESS);

    /* 5) 关键：软件 NSS 主机模式下把内部 NSS 拉高，否则内部 NSS 为低会触发主机模式故障(MODF)。 */
    spi_nss_internal_high(BSP_SPI_PERIPH);

    /* 6) 使能 SPI */
    spi_enable(BSP_SPI_PERIPH);
}

/*!
    \brief      拉低 CS，选中从设备
    \param[in]  none
    \param[out] none
    \retval     none
*/
void bsp_spi_cs_low(void)
{
    gpio_bit_write(BSP_SPI_CS_PORT, BSP_SPI_CS_PIN, BSP_SPI_CS_ACTIVE);
}

/*!
    \brief      拉高 CS，释放从设备
    \param[in]  none
    \param[out] none
    \retval     none
*/
void bsp_spi_cs_high(void)
{
    gpio_bit_write(BSP_SPI_CS_PORT, BSP_SPI_CS_PIN, BSP_SPI_CS_IDLE);
}

/*!
    \brief      全双工交换一个字节
    \param[in]  tx: 要发出的字节
    \param[out] none
    \retval     交换过程中从 MISO 上收到的字节；总线异常超时返回 0xFF
    \note       SPI 是“发一位收一位”同步进行的：写 1 字节触发时钟，时钟走完的同时也收回 1 字节。
                所以即使只想“读”，也要发一个占位字节来产生时钟。
*/
uint8_t bsp_spi_transfer_byte(uint8_t tx)
{
    uint32_t timeout;

    /* 等发送 FIFO 有空位(TBE=发送缓冲空)，再写入待发字节 */
    timeout = BSP_SPI_TIMEOUT;
    while(RESET == spi_i2s_flag_get(BSP_SPI_PERIPH, SPI_FLAG_TBE)) {
        if(0U == --timeout) {
            return BSP_SPI_DUMMY_BYTE;  /* 从设备/总线异常，避免死等 */
        }
    }
    spi_i2s_data_transmit(BSP_SPI_PERIPH, (uint16_t)tx);

    /* 等接收 FIFO 有数据(RBNE=接收缓冲非空)，把同步收回的字节读走 */
    timeout = BSP_SPI_TIMEOUT;
    while(RESET == spi_i2s_flag_get(BSP_SPI_PERIPH, SPI_FLAG_RBNE)) {
        if(0U == --timeout) {
            return BSP_SPI_DUMMY_BYTE;
        }
    }
    return (uint8_t)spi_i2s_data_receive(BSP_SPI_PERIPH);
}

/*!
    \brief      全双工交换一段缓冲区
    \param[in]  tx: 发送数据指针；为 NULL 时每字节发 0xFF 占位（纯读场景）
    \param[in]  len: 字节数
    \param[out] rx: 接收数据写入此处；为 NULL 时丢弃回读（纯写场景）
    \retval     none
    \note       本函数只操作数据，不动 CS。调用前后请自行 bsp_spi_cs_low()/bsp_spi_cs_high()，
                这样一次事务里可以自由拆成多段收发。
*/
void bsp_spi_transfer(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    uint16_t i;
    uint8_t  out;
    uint8_t  in;

    for(i = 0U; i < len; i++) {
        out = (NULL != tx) ? tx[i] : (uint8_t)BSP_SPI_DUMMY_BYTE;
        in  = bsp_spi_transfer_byte(out);
        if(NULL != rx) {
            rx[i] = in;
        }
    }
}

/*!
    \brief      纯写 len 字节（回读丢弃）
    \param[in]  buffer: 发送数据指针
    \param[in]  len: 字节数
    \param[out] none
    \retval     none
*/
void bsp_spi_write(const uint8_t *buffer, uint16_t len)
{
    bsp_spi_transfer(buffer, NULL, len);
}

/*!
    \brief      纯读 len 字节（自动发 0xFF 占位产生时钟）
    \param[in]  len: 字节数
    \param[out] buffer: 接收数据写入此处
    \retval     none
*/
void bsp_spi_read(uint8_t *buffer, uint16_t len)
{
    bsp_spi_transfer(NULL, buffer, len);
}
