# Flash 模拟 EEPROM

## 设计概览

GD32C231F8 主 Flash 为 64 KB、擦除页为 1 KB、编程单位为 8 字节。本模块保留最后两个页：

| 区域 | 地址 | 大小 |
| --- | --- | --- |
| 应用程序 | `0x08000000–0x0800F7FF` | 62 KB |
| Flash EEPROM 页 0 | `0x0800F800–0x0800FBFF` | 1 KB |
| Flash EEPROM 页 1 | `0x0800FC00–0x0800FFFF` | 1 KB |

链接脚本已经把应用区限制为 62 KB，程序增长到保留区时链接会失败，不会静默覆盖参数。

模拟 EEPROM 对外提供 64 字节地址空间。每次数据发生变化时写入一条完整记录，每个 1 KB 页可以存放 11 条记录。记录包含：

- 数据格式版本
- 递增序号
- 64 字节数据
- CRC32
- 最后写入的提交标记

切页时先擦除非活动页，再写新记录；只有记录和 CRC 校验完成后才写提交标记。写入过程中掉电，下一次初始化会忽略未提交记录并恢复上一条有效数据。

## 运行例程

```powershell
cmake --preset FlashEepromDemo
cmake --build --preset FlashEepromDemo --parallel 1
```

烧录 `build/FlashEepromDemo/my-gd32-project_2026-9-3.hex`。串口使用 USART0、PB6/PB7、115200 8N1。

每次复位会读取并递增启动次数，成功时输出：

```text
[FLASH_EEPROM][PASS] boot_count=1 user_value=0x12345678
```

再次复位后 `boot_count` 应变为 2。例程只在 `FlashEepromDemo` 预设中启用，默认 Debug、Release、FactoryTest 不会执行写入。

## 在产品代码中使用

```c
#include "bsp/bsp_flash_eeprom.h"

typedef struct {
    uint32_t device_id;
    uint16_t threshold;
    uint16_t flags;
} product_parameter_t;

product_parameter_t parameter;
bsp_flash_eeprom_status_t status;

status = bsp_flash_eeprom_init();
if(status == BSP_FLASH_EEPROM_OK) {
    status = bsp_flash_eeprom_read(0U, &parameter, sizeof(parameter));
} else if(status == BSP_FLASH_EEPROM_EMPTY) {
    parameter.device_id = 1U;
    parameter.threshold = 100U;
    parameter.flags = 0U;
    status = bsp_flash_eeprom_write(0U, &parameter, sizeof(parameter));
}
```

只更新某个字段也可以使用偏移读写；驱动会在 RAM 中合并，然后把完整 64 字节记录追加到 Flash。

## 状态码

| 状态 | 含义 |
| --- | --- |
| `BSP_FLASH_EEPROM_OK` | 操作成功 |
| `BSP_FLASH_EEPROM_EMPTY` | 尚无有效记录 |
| `BSP_FLASH_EEPROM_INVALID_ARGUMENT` | 地址、长度或指针无效 |
| `BSP_FLASH_EEPROM_NOT_INITIALIZED` | 尚未调用初始化 |
| `BSP_FLASH_EEPROM_FLASH_ERROR` | FMC 擦除或编程失败 |
| `BSP_FLASH_EEPROM_VERIFY_ERROR` | 写后校验失败 |

## 量产注意事项

1. 该模块适合参数、校准值和低频计数，不适合高频日志。
2. 内容不变时驱动不会写 Flash；应用也应避免无意义的周期保存。
3. 擦写期间会短暂关闭中断，不要在实时性要求高的时刻写入。
4. 启用看门狗后，要确认一次页擦除不会触发看门狗复位。
5. 整片擦除会清除模拟 EEPROM。升级工具必须明确选择“保留最后两个页”或先备份再恢复。
6. 写保护、读保护和 Option Bytes 配置必须允许页 62、63 按产品策略操作。
7. 更换芯片容量或重新生成链接脚本后，必须重新核对页大小、地址和 62 KB 应用区限制。
8. Flash 的实际擦写寿命以芯片数据手册为准，不能只依据例程估算量产寿命。
