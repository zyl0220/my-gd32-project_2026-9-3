/*!
    \file    bsp_i2c_eeprom.h
    \brief   I2C 读写 AT24C02 EEPROM 驱动 (GD32C231C-EVAL)

    硬件: I2C1, PA11=SCL / PA12=SDA, AF6, 400kHz, 开漏+上拉
    器件: AT24C02 (256B, 页大小 8B, 单字节内部地址, 从地址 0xA0)
    实现参考: GD32C2x1_Firmware_Library_V1.3.0 / Examples/I2C/I2C_EEPROM

    \note 本模块为项目自有板级代码，与厂商库隔离，由 CMake 自动编译。
*/

#ifndef BSP_I2C_EEPROM_H
#define BSP_I2C_EEPROM_H

#include <stdint.h>

/* ---- 硬件资源 (按官方 EVAL 板定义, 换板改这里) ---- */
#define BSP_EEPROM_I2C_SPEED        400000U
#define BSP_EEPROM_PAGE_SIZE    8U
#define BSP_EEPROM_I2C_PERIPH             I2C1
#define BSP_EEPROM_GPIO_RCU     RCU_GPIOA
#define BSP_EEPROM_I2C_RCU          RCU_I2C1
#define BSP_EEPROM_SCL_PORT     GPIOA
#define BSP_EEPROM_SDA_PORT     GPIOA
#define BSP_EEPROM_SCL_PIN      GPIO_PIN_11
#define BSP_EEPROM_SDA_PIN      GPIO_PIN_12
#define BSP_EEPROM_GPIO_AF      GPIO_AF_6

/* ---- 器件 ---- */
#define BSP_EEPROM_ADDRESS   0xA0U   /* AT24C02 块0 从地址 */

#define BSP_EEPROM_OK           0U
#define BSP_EEPROM_FAIL         1U

/* ---- 对外 API ---- */
/* 初始化 I2C1 引脚 + 控制器 + EEPROM 驱动 (main 早期调用一次) */
void    bsp_i2c_eeprom_init(void);
/* 向 EEPROM 写入 number_of_byte 字节(自动按页处理跨页) */
void    bsp_eeprom_write(uint8_t *p_buffer, uint8_t write_address, uint16_t number_of_byte);
/* 从 EEPROM 读出 number_of_byte 字节 */
void    bsp_eeprom_read(uint8_t *p_buffer, uint8_t read_address, uint16_t number_of_byte);
/* 写入-读回-比对 自测, 返回 BSP_EEPROM_OK / BSP_EEPROM_FAIL */
uint8_t bsp_i2c_eeprom_selftest(void);

#endif /* BSP_I2C_EEPROM_H */
