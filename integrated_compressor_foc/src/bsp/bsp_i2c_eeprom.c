/*!
    \file    bsp_i2c_eeprom.c
    \brief   I2C 读写 AT24C02 EEPROM 驱动实现

    状态机(eeprom_page_write / bsp_eeprom_read)忠实移植自官方例程
    GD32C2x1_Firmware_Library_V1.3.0 / Examples/I2C/I2C_EEPROM/at24cxx.c,
    仅做模块化封装与命名规整, I2C 时序/标志位序列未改动。
*/

#include "bsp/bsp_i2c_eeprom.h"
#include "gd32c2x1.h"
#include "systick.h"
#include <stdio.h>

#define I2C_TIME_OUT     ((uint32_t)20000)
#define MAX_RELOAD_SIZE  255U

typedef enum {
    I2C_START = 0,
    I2C_SEND_ADDRESS,
    I2C_RESTART,
    I2C_TRANSMIT_DATA,
    I2C_RELOAD,
    I2C_STOP,
    I2C_END
} i2c_process_enum;

static uint16_t s_eeprom_address;   /* EEPROM 从地址 */

/* ---- 内部: 引脚配置 ---- */
static void i2c_hw_gpio_config(void)
{
    rcu_periph_clock_enable(BSP_EEPROM_GPIO_RCU);
    rcu_periph_clock_enable(BSP_EEPROM_I2C_RCU);
    gpio_af_set(BSP_EEPROM_SCL_PORT, BSP_EEPROM_GPIO_AF, BSP_EEPROM_SCL_PIN);
    gpio_af_set(BSP_EEPROM_SDA_PORT, BSP_EEPROM_GPIO_AF, BSP_EEPROM_SDA_PIN);
    gpio_mode_set(BSP_EEPROM_SCL_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, BSP_EEPROM_SCL_PIN);
    gpio_output_options_set(BSP_EEPROM_SCL_PORT, GPIO_OTYPE_OD, GPIO_OSPEED_LEVEL_1, BSP_EEPROM_SCL_PIN);
    gpio_mode_set(BSP_EEPROM_SDA_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, BSP_EEPROM_SDA_PIN);
    gpio_output_options_set(BSP_EEPROM_SDA_PORT, GPIO_OTYPE_OD, GPIO_OSPEED_LEVEL_1, BSP_EEPROM_SDA_PIN);
}

/* ---- 内部: 控制器配置(时序参数同官方 400kHz) ---- */
static void i2c_hw_config(void)
{
    i2c_timing_config(BSP_EEPROM_I2C_PERIPH, 0, 0x4, 0);
    i2c_master_clock_config(BSP_EEPROM_I2C_PERIPH, 0x1C, 0x55);
    i2c_enable(BSP_EEPROM_I2C_PERIPH);
}

/* ---- 内部: 总线卡死时复位(SDA 被从机拉死的救援) ---- */
static void i2c_bus_reset(void)
{
    GPIO_BC(BSP_EEPROM_SCL_PORT) |= BSP_EEPROM_SCL_PIN;
    GPIO_BC(BSP_EEPROM_SDA_PORT) |= BSP_EEPROM_SDA_PIN;
    gpio_mode_set(BSP_EEPROM_SCL_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, BSP_EEPROM_SCL_PIN);
    gpio_output_options_set(BSP_EEPROM_SCL_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_LEVEL_1, BSP_EEPROM_SCL_PIN);
    gpio_mode_set(BSP_EEPROM_SDA_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, BSP_EEPROM_SDA_PIN);
    gpio_output_options_set(BSP_EEPROM_SDA_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_LEVEL_1, BSP_EEPROM_SDA_PIN);
    __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
    GPIO_BOP(BSP_EEPROM_SCL_PORT) |= BSP_EEPROM_SCL_PIN;
    __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
    GPIO_BOP(BSP_EEPROM_SDA_PORT) |= BSP_EEPROM_SDA_PIN;
    gpio_mode_set(BSP_EEPROM_SCL_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, BSP_EEPROM_SCL_PIN);
    gpio_output_options_set(BSP_EEPROM_SCL_PORT, GPIO_OTYPE_OD, GPIO_OSPEED_LEVEL_1, BSP_EEPROM_SCL_PIN);
    gpio_mode_set(BSP_EEPROM_SDA_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, BSP_EEPROM_SDA_PIN);
    gpio_output_options_set(BSP_EEPROM_SDA_PORT, GPIO_OTYPE_OD, GPIO_OSPEED_LEVEL_1, BSP_EEPROM_SDA_PIN);
}

/* ---- 内部: 一个写周期内写入 <= 一页 字节 ---- */
static void eeprom_page_write(uint8_t *p_buffer, uint8_t write_address, uint8_t number_of_byte)
{
    i2c_process_enum state = I2C_START;
    uint16_t timeout = 0;
    uint8_t end_flag = 0;

    while(!end_flag) {
        switch(state) {
        case I2C_START:
            i2c_master_addressing(BSP_EEPROM_I2C_PERIPH, s_eeprom_address, I2C_MASTER_TRANSMIT);
            i2c_transfer_byte_number_config(BSP_EEPROM_I2C_PERIPH, number_of_byte + 1);
            I2C_STAT(BSP_EEPROM_I2C_PERIPH) |= I2C_STAT_TBE;
            i2c_automatic_end_enable(BSP_EEPROM_I2C_PERIPH);
            while(i2c_flag_get(BSP_EEPROM_I2C_PERIPH, I2C_FLAG_I2CBSY) && (timeout < I2C_TIME_OUT)) {
                timeout++;
            }
            if(timeout < I2C_TIME_OUT) {
                i2c_start_on_bus(BSP_EEPROM_I2C_PERIPH);
                timeout = 0;
                state = I2C_SEND_ADDRESS;
            } else {
                i2c_bus_reset();
                timeout = 0;
                state = I2C_START;
                printf("[I2C] bus busy in page write!\r\n");
            }
            break;
        case I2C_SEND_ADDRESS:
            while((!i2c_flag_get(BSP_EEPROM_I2C_PERIPH, I2C_FLAG_TBE)) && (timeout < I2C_TIME_OUT)) {
                timeout++;
            }
            if(timeout < I2C_TIME_OUT) {
                i2c_data_transmit(BSP_EEPROM_I2C_PERIPH, write_address);
                timeout = 0;
                state = I2C_TRANSMIT_DATA;
            } else {
                timeout = 0;
                state = I2C_START;
                printf("[I2C] send addr timeout in page write!\r\n");
            }
            break;
        case I2C_TRANSMIT_DATA:
            while(number_of_byte--) {
                while((!i2c_flag_get(BSP_EEPROM_I2C_PERIPH, I2C_FLAG_TI)) && (timeout < I2C_TIME_OUT)) {
                    timeout++;
                }
                if(timeout < I2C_TIME_OUT) {
                    i2c_data_transmit(BSP_EEPROM_I2C_PERIPH, *p_buffer);
                    p_buffer++;
                    timeout = 0;
                    state = I2C_STOP;
                } else {
                    timeout = 0;
                    state = I2C_START;
                    printf("[I2C] send data timeout in page write!\r\n");
                    return;
                }
            }
            break;
        case I2C_STOP:
            while((!i2c_flag_get(BSP_EEPROM_I2C_PERIPH, I2C_FLAG_STPDET)) && (timeout < I2C_TIME_OUT)) {
                timeout++;
            }
            if(timeout < I2C_TIME_OUT) {
                i2c_flag_clear(BSP_EEPROM_I2C_PERIPH, I2C_FLAG_STPDET);
                timeout = 0;
                state = I2C_END;
                end_flag = 1;
            } else {
                timeout = 0;
                state = I2C_START;
                printf("[I2C] stop timeout in page write!\r\n");
            }
            break;
        default:
            state = I2C_START;
            end_flag = 1;
            timeout = 0;
            printf("[I2C] bad state in page write!\r\n");
            break;
        }
    }
}

void bsp_eeprom_write(uint8_t *p_buffer, uint8_t write_address, uint16_t number_of_byte)
{
    uint8_t number_of_page, number_of_single, address, count;

    address = write_address % BSP_EEPROM_PAGE_SIZE;
    count = BSP_EEPROM_PAGE_SIZE - address;
    number_of_page = number_of_byte / BSP_EEPROM_PAGE_SIZE;
    number_of_single = number_of_byte % BSP_EEPROM_PAGE_SIZE;

    if(0 == address) {
        /* 写起始地址按页对齐 */
        while(number_of_page--) {
            eeprom_page_write(p_buffer, write_address, BSP_EEPROM_PAGE_SIZE);
            delay_1ms(5);
            write_address += BSP_EEPROM_PAGE_SIZE;
            p_buffer += BSP_EEPROM_PAGE_SIZE;
        }
        if(0 != number_of_single) {
            eeprom_page_write(p_buffer, write_address, number_of_single);
            delay_1ms(5);
        }
    } else {
        /* 写起始地址未按页对齐 */
        if(number_of_byte < count) {
            eeprom_page_write(p_buffer, write_address, number_of_byte);
            delay_1ms(5);
        } else {
            number_of_byte -= count;
            number_of_page = number_of_byte / BSP_EEPROM_PAGE_SIZE;
            number_of_single = number_of_byte % BSP_EEPROM_PAGE_SIZE;
            if(0 != count) {
                eeprom_page_write(p_buffer, write_address, count);
                delay_1ms(5);
                write_address += count;
                p_buffer += count;
            }
            while(number_of_page--) {
                eeprom_page_write(p_buffer, write_address, BSP_EEPROM_PAGE_SIZE);
                delay_1ms(5);
                write_address += BSP_EEPROM_PAGE_SIZE;
                p_buffer += BSP_EEPROM_PAGE_SIZE;
            }
            if(0 != number_of_single) {
                eeprom_page_write(p_buffer, write_address, number_of_single);
                delay_1ms(5);
            }
        }
    }
}

void bsp_eeprom_read(uint8_t *p_buffer, uint8_t read_address, uint16_t number_of_byte)
{
    uint32_t nbytes_reload = 0;
    i2c_process_enum state = I2C_START;
    uint32_t timeout = 0;
    uint8_t end_flag = 0;
    uint8_t restart_flag = 0;
    uint8_t first_reload_flag = 1;

    while(!end_flag) {
        switch(state) {
        case I2C_START:
            if(0 == restart_flag) {
                I2C_STAT(BSP_EEPROM_I2C_PERIPH) |= I2C_STAT_TBE;
                i2c_master_addressing(BSP_EEPROM_I2C_PERIPH, s_eeprom_address, I2C_MASTER_TRANSMIT);
                i2c_transfer_byte_number_config(BSP_EEPROM_I2C_PERIPH, 1);
                i2c_automatic_end_disable(BSP_EEPROM_I2C_PERIPH);
                while(i2c_flag_get(BSP_EEPROM_I2C_PERIPH, I2C_FLAG_I2CBSY) && (timeout < I2C_TIME_OUT)) {
                    timeout++;
                }
                if(timeout < I2C_TIME_OUT) {
                    i2c_start_on_bus(BSP_EEPROM_I2C_PERIPH);
                    timeout = 0;
                    state = I2C_SEND_ADDRESS;
                } else {
                    i2c_bus_reset();
                    timeout = 0;
                    state = I2C_START;
                    printf("[I2C] bus busy in read!\r\n");
                }
            } else {
                i2c_start_on_bus(BSP_EEPROM_I2C_PERIPH);
                restart_flag = 0;
                state = I2C_TRANSMIT_DATA;
            }
            break;
        case I2C_SEND_ADDRESS:
            while((!i2c_flag_get(BSP_EEPROM_I2C_PERIPH, I2C_FLAG_TBE)) && (timeout < I2C_TIME_OUT)) {
                timeout++;
            }
            if(timeout < I2C_TIME_OUT) {
                i2c_data_transmit(BSP_EEPROM_I2C_PERIPH, read_address);
                timeout = 0;
                state = I2C_RESTART;
            } else {
                timeout = 0;
                state = I2C_START;
                printf("[I2C] send addr timeout in read!\r\n");
            }
            break;
        case I2C_RESTART:
            while((!i2c_flag_get(BSP_EEPROM_I2C_PERIPH, I2C_FLAG_TC)) && (timeout < I2C_TIME_OUT)) {
                timeout++;
            }
            if(timeout < I2C_TIME_OUT) {
                i2c_master_addressing(BSP_EEPROM_I2C_PERIPH, s_eeprom_address, I2C_MASTER_RECEIVE);
                i2c_reload_enable(BSP_EEPROM_I2C_PERIPH);
                timeout = 0;
                state = I2C_RELOAD;
                restart_flag = 1;
            } else {
                timeout = 0;
                state = I2C_START;
                printf("[I2C] internal addr timeout in read!\r\n");
            }
            break;
        case I2C_RELOAD:
            if(number_of_byte > MAX_RELOAD_SIZE) {
                number_of_byte = number_of_byte - MAX_RELOAD_SIZE;
                nbytes_reload = MAX_RELOAD_SIZE;
            } else {
                nbytes_reload = number_of_byte;
            }
            if(1 == first_reload_flag) {
                i2c_transfer_byte_number_config(BSP_EEPROM_I2C_PERIPH, nbytes_reload);
                first_reload_flag = 0;
                state = I2C_START;
            } else {
                while((!i2c_flag_get(BSP_EEPROM_I2C_PERIPH, I2C_FLAG_TCR)) && (timeout < I2C_TIME_OUT)) {
                    timeout++;
                }
                if(timeout < I2C_TIME_OUT) {
                    i2c_transfer_byte_number_config(BSP_EEPROM_I2C_PERIPH, nbytes_reload);
                    if(number_of_byte <= MAX_RELOAD_SIZE) {
                        i2c_reload_disable(BSP_EEPROM_I2C_PERIPH);
                        i2c_automatic_end_enable(BSP_EEPROM_I2C_PERIPH);
                    }
                    timeout = 0;
                    state = I2C_TRANSMIT_DATA;
                } else {
                    timeout = 0;
                    state = I2C_START;
                    printf("[I2C] reload timeout in read!\r\n");
                }
            }
            break;
        case I2C_TRANSMIT_DATA:
            while((!i2c_flag_get(BSP_EEPROM_I2C_PERIPH, I2C_FLAG_TBE)) && (timeout < I2C_TIME_OUT)) {
                timeout++;
            }
            if(timeout < I2C_TIME_OUT) {
                while(nbytes_reload) {
                    if(i2c_flag_get(BSP_EEPROM_I2C_PERIPH, I2C_FLAG_RBNE)) {
                        *p_buffer = i2c_data_receive(BSP_EEPROM_I2C_PERIPH);
                        p_buffer++;
                        nbytes_reload--;
                    }
                }
                timeout = 0;
                if(I2C_CTL1(BSP_EEPROM_I2C_PERIPH) & I2C_CTL1_RELOAD) {
                    state = I2C_RELOAD;
                } else {
                    state = I2C_STOP;
                }
            } else {
                timeout = 0;
                state = I2C_START;
                printf("[I2C] read data timeout in read!\r\n");
            }
            break;
        case I2C_STOP:
            while((!i2c_flag_get(BSP_EEPROM_I2C_PERIPH, I2C_FLAG_STPDET)) && (timeout < I2C_TIME_OUT)) {
                timeout++;
            }
            if(timeout < I2C_TIME_OUT) {
                i2c_flag_clear(BSP_EEPROM_I2C_PERIPH, I2C_FLAG_STPDET);
                timeout = 0;
                state = I2C_END;
                end_flag = 1;
            } else {
                timeout = 0;
                state = I2C_START;
                printf("[I2C] stop timeout in read!\r\n");
            }
            break;
        default:
            state = I2C_START;
            end_flag = 1;
            timeout = 0;
            printf("[I2C] bad state in read!\r\n");
            break;
        }
    }
}

void bsp_i2c_eeprom_init(void)
{
    i2c_hw_gpio_config();
    i2c_hw_config();
    s_eeprom_address = BSP_EEPROM_ADDRESS;
}

uint8_t bsp_i2c_eeprom_selftest(void)
{
    /* 用 16 字节(2页)做写-读-比对, 含一个固定特征头便于人工核对 */
    uint8_t wr[16];
    uint8_t rd[16];
    uint16_t i;

    for(i = 0; i < sizeof(wr); i++) {
        wr[i] = (uint8_t)(0xA0U ^ i);   /* 0xA0,0xA1,0xA3,... 易辨认的图案 */
    }

    printf("[EEPROM] write 16B @ page0 ...\r\n");
    bsp_eeprom_write(wr, 0x00U, sizeof(wr));

    for(i = 0; i < sizeof(rd); i++) {
        rd[i] = 0x00U;
    }
    printf("[EEPROM] read back ...\r\n");
    bsp_eeprom_read(rd, 0x00U, sizeof(rd));

    for(i = 0; i < sizeof(wr); i++) {
        if(rd[i] != wr[i]) {
            printf("[EEPROM][FAIL] @%u wr=0x%02X rd=0x%02X\r\n", i, wr[i], rd[i]);
            return BSP_EEPROM_FAIL;
        }
    }
    printf("[EEPROM][PASS] 16B write/read verified\r\n");
    return BSP_EEPROM_OK;
}
