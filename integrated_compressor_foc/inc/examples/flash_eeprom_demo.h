/**
 * @file flash_eeprom_demo.h
 * @brief Flash 模拟 EEPROM 上电读写例程。
 */

#ifndef FLASH_EEPROM_DEMO_H
#define FLASH_EEPROM_DEMO_H

#ifdef __cplusplus
extern "C" {
#endif

/* 每次上电调用一次：读取、递增并写回启动计数，然后读回校验。 */
void flash_eeprom_demo_run(void);

#ifdef __cplusplus
}
#endif

#endif /* FLASH_EEPROM_DEMO_H */
