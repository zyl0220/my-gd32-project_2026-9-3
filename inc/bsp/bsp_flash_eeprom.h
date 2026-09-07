/**
 * @file bsp_flash_eeprom.h
 * @brief 使用片内 Flash 模拟 64 字节 EEPROM。
 *
 * 存储区使用主 Flash 最后两个 1 KB 页。每次更新追加一条带序号、CRC32
 * 和提交标记的完整记录；当前页写满后切换到另一页，以降低擦写次数并提供
 * 掉电恢复能力。
 *
 * 调用约定：先 init，再 read/write；由同一个主循环/任务串行调用。
 * 驱动无互斥锁，不可在 ISR 或多个任务中并发使用；擦写为同步阻塞操作。
 * 它不会自动在断电时保存，应用必须主动调用 write 并检查返回值。
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
 *         BSP_FLASH_EEPROM_EMPTY：没有通过校验的记录（空白或记录均无效）；
 *         其他值：初始化失败。
 * 不擦写 Flash；即使返回 EMPTY 也已初始化，可以继续首次 write。
 */
bsp_flash_eeprom_status_t bsp_flash_eeprom_init(void);

/**
 * 从模拟 EEPROM 读取数据。调用前必须先执行 bsp_flash_eeprom_init()。
 * offset/length 均以字节计，操作区间必须位于 64 字节逻辑空间内。
 * data 指向调用者的接收缓冲区；读取来自 RAM 缓存，无有效数据返回 EMPTY。
 */
bsp_flash_eeprom_status_t bsp_flash_eeprom_read(uint16_t offset,
                                                 void *data,
                                                 uint16_t length);

/**
 * 更新模拟 EEPROM。内容没有变化时不会写 Flash。
 *
 * 每次成功更新都会写入一条完整记录，适合参数、校准值等低频数据，
 * 不适合毫秒级计数或日志。
 * offset 为逻辑偏移，data 内容复制到指定区间，其余字节沿用旧快照。
 * 返回 OK 表示内容原本相同、零长度请求，或新记录已写入且校验通过。
 * 多个相关字段应一次提交；多次 write 之间掉电可能仅保存前几次修改。
 */
bsp_flash_eeprom_status_t bsp_flash_eeprom_write(uint16_t offset,
                                                  const void *data,
                                                  uint16_t length);

/**
 * 擦除两个保留页。该操作会删除全部模拟 EEPROM 数据。
 * 需要先 init；不用于普通保存，格式化中断不能保证旧数据仍然完整。
 */
bsp_flash_eeprom_status_t bsp_flash_eeprom_format(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_FLASH_EEPROM_H */
