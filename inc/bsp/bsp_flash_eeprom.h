/**
 * @file bsp_flash_eeprom.h
 * @brief 使用片内 Flash 模拟 64 字节 EEPROM。
 *
 * 存储区使用主 Flash 最后两个 1 KB 页。每次更新追加一条带序号、CRC32
 * 和提交标记的完整记录；当前页写满后切换到另一页，以降低擦写次数并提供
 * 掉电恢复能力。
 */

#ifndef BSP_FLASH_EEPROM_H
#define BSP_FLASH_EEPROM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define BSP_FLASH_EEPROM_CAPACITY 64U

typedef enum {
    BSP_FLASH_EEPROM_OK = 0,
    BSP_FLASH_EEPROM_EMPTY,
    BSP_FLASH_EEPROM_INVALID_ARGUMENT,
    BSP_FLASH_EEPROM_NOT_INITIALIZED,
    BSP_FLASH_EEPROM_FLASH_ERROR,
    BSP_FLASH_EEPROM_VERIFY_ERROR
} bsp_flash_eeprom_status_t;

/**
 * 扫描两个保留页并恢复最新有效记录。
 *
 * @return BSP_FLASH_EEPROM_OK：找到有效数据；
 *         BSP_FLASH_EEPROM_EMPTY：存储区尚未写入；
 *         其他值：初始化失败。
 */
bsp_flash_eeprom_status_t bsp_flash_eeprom_init(void);

/**
 * 从模拟 EEPROM 读取数据。调用前必须先执行 bsp_flash_eeprom_init()。
 */
bsp_flash_eeprom_status_t bsp_flash_eeprom_read(uint16_t offset,
                                                 void *data,
                                                 uint16_t length);

/**
 * 更新模拟 EEPROM。内容没有变化时不会写 Flash。
 *
 * 每次成功更新都会写入一条完整记录，适合参数、校准值等低频数据，
 * 不适合毫秒级计数或日志。
 */
bsp_flash_eeprom_status_t bsp_flash_eeprom_write(uint16_t offset,
                                                  const void *data,
                                                  uint16_t length);

/**
 * 擦除两个保留页。该操作会删除全部模拟 EEPROM 数据。
 */
bsp_flash_eeprom_status_t bsp_flash_eeprom_format(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_FLASH_EEPROM_H */
