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

/* 这是 payload 内的应用格式（16字节）；与驱动记录头的 magic/version 不同。
 * 驱动检查存储完整性，应用检查读出的数据是否属于自己的业务格式。 */
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

    /* 第一步：扫描恢复。EMPTY 表示没有有效记录，不会自动擦除或格式化。 */
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
        /* 存储完整不代表业务格式兼容；例程遇到不匹配只报错，避免覆盖别的数据。 */
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

    /* 第二步：修改并同步保存。本例每次启动写一次，用来演示跨复位记忆。
     * 产品的模式保存应由用户确认修改触发，而不是照搬启动计数行为。 */
    data.boot_count++;
    status = bsp_flash_eeprom_write(0U, &data, sizeof(data));
    if(status != BSP_FLASH_EEPROM_OK) {
        flash_eeprom_demo_print_error("write", status);
        return;
    }

    /* 第三步：验证应用读取接口（这里读的是 RAM 缓存）。
     * 驱动 write 内部已做 Flash 回读校验；真实掉电保持需断电重启后验证。 */
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
