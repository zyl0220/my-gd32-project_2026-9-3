/**
 * @file flash_eeprom_demo.c
 * @brief Flash 模拟 EEPROM 的最小使用示例。
 */

#include "examples/flash_eeprom_demo.h"

#include <stdio.h>
#include <string.h>
#include "bsp/bsp_flash_eeprom.h"

#define FLASH_EEPROM_DEMO_MAGIC    0x4F4D4544UL /* "DEMO" little-endian */
#define FLASH_EEPROM_DEMO_VERSION  1U
#define FLASH_EEPROM_DEMO_VALUE    0x12345678UL

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    uint32_t boot_count;
    uint32_t user_value;
} flash_eeprom_demo_data_t;

_Static_assert(sizeof(flash_eeprom_demo_data_t) <= BSP_FLASH_EEPROM_CAPACITY,
               "Demo data exceeds Flash EEPROM capacity");

static void flash_eeprom_demo_print_error(const char *operation,
                                           bsp_flash_eeprom_status_t status)
{
    printf("[FLASH_EEPROM][FAIL] %s, status=%u\r\n",
           operation,
           (unsigned int)status);
}

void flash_eeprom_demo_run(void)
{
    flash_eeprom_demo_data_t data;
    flash_eeprom_demo_data_t verify;
    bsp_flash_eeprom_status_t status;

    status = bsp_flash_eeprom_init();
    if(status == BSP_FLASH_EEPROM_EMPTY) {
        memset(&data, 0, sizeof(data));
        data.magic = FLASH_EEPROM_DEMO_MAGIC;
        data.version = FLASH_EEPROM_DEMO_VERSION;
        data.user_value = FLASH_EEPROM_DEMO_VALUE;
    } else if(status == BSP_FLASH_EEPROM_OK) {
        status = bsp_flash_eeprom_read(0U, &data, sizeof(data));
        if(status != BSP_FLASH_EEPROM_OK) {
            flash_eeprom_demo_print_error("read", status);
            return;
        }
        if((data.magic != FLASH_EEPROM_DEMO_MAGIC) ||
           (data.version != FLASH_EEPROM_DEMO_VERSION)) {
            printf("[FLASH_EEPROM][FAIL] data schema mismatch; "
                   "format explicitly before reusing this area\r\n");
            return;
        }
    } else {
        flash_eeprom_demo_print_error("init", status);
        return;
    }

    data.boot_count++;
    status = bsp_flash_eeprom_write(0U, &data, sizeof(data));
    if(status != BSP_FLASH_EEPROM_OK) {
        flash_eeprom_demo_print_error("write", status);
        return;
    }

    memset(&verify, 0, sizeof(verify));
    status = bsp_flash_eeprom_read(0U, &verify, sizeof(verify));
    if(status != BSP_FLASH_EEPROM_OK) {
        flash_eeprom_demo_print_error("verify read", status);
        return;
    }
    if(0 != memcmp(&data, &verify, sizeof(data))) {
        printf("[FLASH_EEPROM][FAIL] verify mismatch\r\n");
        return;
    }

    printf("[FLASH_EEPROM][PASS] boot_count=%lu user_value=0x%08lX\r\n",
           (unsigned long)verify.boot_count,
           (unsigned long)verify.user_value);
}
