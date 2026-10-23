/**
 * @file bsp_flash_eeprom.c
 * @brief GD32C231 双页日志式 Flash EEPROM 实现。
 *
 * 对外是 64 字节逻辑空间，对内是两页交替追加的完整快照。
 * 学习顺序：record 布局 -> init 扫描恢复 -> read 缓存读取 -> write 生成快照
 * -> program_record 最后提交 -> erase_page 回收非活动页。
 * 本模块不检测掉电，也不决定何时保存；这些策略由应用层负责。
 */

#include "bsp/bsp_flash_eeprom.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "gd32c2x1.h"
#include "gd32c2x1_fmc.h"

/* 逻辑页 0/1 对应芯片物理页 62/63；须与链接脚本的末尾 2KB 保留区一致。 */
#define FLASH_EEPROM_PAGE_COUNT          2U
#define FLASH_EEPROM_PAGE0_NUMBER        (MAIN_FLASH_PAGE_TOTAL_NUM - FLASH_EEPROM_PAGE_COUNT)
#define FLASH_EEPROM_PAGE1_NUMBER        (MAIN_FLASH_PAGE_TOTAL_NUM - 1U)
#define FLASH_EEPROM_PAGE0_ADDRESS       (MAIN_FLASH_BASE_ADDRESS + +                                          (FLASH_EEPROM_PAGE0_NUMBER * MAIN_FLASH_PAGE_SIZE))
#define FLASH_EEPROM_PAGE1_ADDRESS       (MAIN_FLASH_BASE_ADDRESS + +                                          (FLASH_EEPROM_PAGE1_NUMBER * MAIN_FLASH_PAGE_SIZE))
#define FLASH_EEPROM_RECORD_MAGIC        0x52504545UL /* "EEPR" little-endian */
#define FLASH_EEPROM_FORMAT_VERSION      1U
#define FLASH_EEPROM_COMMIT_MARKER       UINT64_C(0xC35AC35A5AA5A55A)
#define FLASH_EEPROM_ERASED_DOUBLEWORD   UINT64_MAX
#define FLASH_EEPROM_INVALID_PAGE        0xFFU
#define FLASH_EEPROM_INVALID_SLOT        0xFFFFU

/* 当前布局（单位：字节）：
 *   0 magic(4) | 4 version(2) | 6 size(2) | 8 sequence(4) | 12 crc32(4)
 *  16 payload(64) | 80 commit(8)，共 88 字节，按 8 字节编程。
 * 每页 1024 / 88 = 11 条记录，末尾剩余 56 字节不使用。
 * payload 是应用数据；sequence/CRC/commit 是驱动元数据，不占逻辑容量。
 * 这是持久化格式，修改布局需同时考虑已写入旧固件数据的兼容性。 */
typedef struct __attribute__((aligned(8))) {
    uint32_t magic;
    uint16_t format_version;
    uint16_t payload_size;
    uint32_t sequence;
    uint32_t crc32;
    uint8_t payload[BSP_FLASH_EEPROM_CAPACITY];
    uint64_t commit;
} flash_eeprom_record_t;

#define FLASH_EEPROM_RECORD_SIZE         ((uint32_t)sizeof(flash_eeprom_record_t))
#define FLASH_EEPROM_RECORDS_PER_PAGE    (MAIN_FLASH_PAGE_SIZE / FLASH_EEPROM_RECORD_SIZE)
#define FLASH_EEPROM_PROGRAM_WORDS       (FLASH_EEPROM_RECORD_SIZE / sizeof(uint64_t))
#define FLASH_EEPROM_COMMIT_WORD_INDEX   (FLASH_EEPROM_PROGRAM_WORDS - 1U)

_Static_assert((sizeof(flash_eeprom_record_t) % sizeof(uint64_t)) == 0U,
               "Flash EEPROM record must be 8-byte aligned in size");
_Static_assert(_Alignof(flash_eeprom_record_t) >= sizeof(uint64_t),
               "Flash EEPROM record must be 8-byte aligned in memory");
_Static_assert(offsetof(flash_eeprom_record_t, commit) ==
               (sizeof(flash_eeprom_record_t) - sizeof(uint64_t)),
               "Commit marker must be the final doubleword");
_Static_assert(FLASH_EEPROM_RECORDS_PER_PAGE > 1U,
               "Flash EEPROM page must hold multiple records");
_Static_assert(FLASH_EEPROM_PAGE0_ADDRESS == 0x0800F800UL,
               "Linker reservation and GD32C231 flash geometry disagree");
_Static_assert(FLASH_EEPROM_PAGE1_ADDRESS == 0x0800FC00UL,
               "Linker reservation and GD32C231 flash geometry disagree");

/* RAM 保存最近一次恢复/成功写入的快照，掉电后由 init() 从 Flash 重建。
 * active_page 指最新有效记录所在页；initialized 与 has_data 分开，
 * 因为“完成初始化但没有有效记录”也是允许首次写入的状态。 */
static uint8_t g_flash_eeprom_cache[BSP_FLASH_EEPROM_CAPACITY];
static uint32_t g_flash_eeprom_sequence;
static uint8_t g_flash_eeprom_active_page;
static bool g_flash_eeprom_initialized;
static bool g_flash_eeprom_has_data;

static uint32_t flash_eeprom_page_address(uint8_t page)
{
    return (page == 0U) ? FLASH_EEPROM_PAGE0_ADDRESS : FLASH_EEPROM_PAGE1_ADDRESS;
}

static uint32_t flash_eeprom_page_number(uint8_t page)
{
    return (page == 0U) ? FLASH_EEPROM_PAGE0_NUMBER : FLASH_EEPROM_PAGE1_NUMBER;
}

static const flash_eeprom_record_t *flash_eeprom_record(uint8_t page, uint16_t slot)
{
    /* 片内 Flash 可按地址直接读；这里只取得只读指针，写入必须通过 FMC。 */
    uint32_t address = flash_eeprom_page_address(page) +
                       ((uint32_t)slot * FLASH_EEPROM_RECORD_SIZE);
    return (const flash_eeprom_record_t *)(uintptr_t)address;
}

static uint32_t flash_eeprom_crc32_update(uint32_t crc,
                                          const uint8_t *data,
                                          uint32_t length)
{
    uint32_t index;
    uint8_t bit;

    for(index = 0U; index < length; index++) {
        crc ^= data[index];
        for(bit = 0U; bit < 8U; bit++) {
            if((crc & 1U) != 0U) {
                crc = (crc >> 1U) ^ 0xEDB88320UL;
            } else {
                crc >>= 1U;
            }
        }
    }

    return crc;
}

static uint32_t flash_eeprom_record_crc(const flash_eeprom_record_t *record)
{
    /* CRC 覆盖序号和全部 64 字节数据；固定头字段另作等值检查。
     * 不把 crc32 自身和 commit 算进去，避免自引用；CRC 用于检错，不是加密。 */
    uint32_t crc = UINT32_MAX;

    crc = flash_eeprom_crc32_update(
        crc, (const uint8_t *)&record->sequence, sizeof(record->sequence));
    crc = flash_eeprom_crc32_update(crc, record->payload, sizeof(record->payload));
    return crc ^ UINT32_MAX;
}

static bool flash_eeprom_record_is_valid(const flash_eeprom_record_t *record)
{
    if((record->commit != FLASH_EEPROM_COMMIT_MARKER) ||
       (record->magic != FLASH_EEPROM_RECORD_MAGIC) ||
       (record->format_version != FLASH_EEPROM_FORMAT_VERSION) ||
       (record->payload_size != BSP_FLASH_EEPROM_CAPACITY) ||
       (record->sequence == 0U)) {
        return false;
    }

    return record->crc32 == flash_eeprom_record_crc(record);
}

static bool flash_eeprom_record_is_erased(const flash_eeprom_record_t *record)
{
    /* “无效记录”不等于“空槽”：半写记录可能已有 0 位，不能直接再次编程。
     * 当前实现检查整条记录是否读出全 1；异常断电后的硬件行为仍需实板验证。 */
    const uint64_t *word = (const uint64_t *)(const void *)record;
    uint32_t index;

    for(index = 0U; index < FLASH_EEPROM_PROGRAM_WORDS; index++) {
        if(word[index] != FLASH_EEPROM_ERASED_DOUBLEWORD) {
            return false;
        }
    }

    return true;
}

static bool flash_eeprom_sequence_is_newer(uint32_t candidate, uint32_t reference)
{
    /* 在本工程 GCC/32位整数下用模差处理回绕；比较前提是序号间距小于 2^31。
     * 例如 reference=0xFFFFFFFF，candidate=1，应判 candidate 更新。 */
    return (int32_t)(candidate - reference) > 0;
}

static uint16_t flash_eeprom_find_erased_slot(uint8_t page)
{
    uint16_t slot;

    for(slot = 0U; slot < FLASH_EEPROM_RECORDS_PER_PAGE; slot++) {
        if(flash_eeprom_record_is_erased(flash_eeprom_record(page, slot))) {
            return slot;
        }
    }

    return FLASH_EEPROM_INVALID_SLOT;
}

static void flash_eeprom_clear_fmc_flags(void)
{
    fmc_flag_clear(FMC_FLAG_ENDF);
    fmc_flag_clear(FMC_FLAG_OPRERR);
    fmc_flag_clear(FMC_FLAG_PGERR);
    fmc_flag_clear(FMC_FLAG_WPERR);
    fmc_flag_clear(FMC_FLAG_PGAERR);
    fmc_flag_clear(FMC_FLAG_PGMERR);
    fmc_flag_clear(FMC_FLAG_PGSERR);
    fmc_flag_clear(FMC_FLAG_FSTPERR);
    fmc_flag_clear(FMC_FLAG_RPERR);
}

static uint32_t flash_eeprom_irq_save(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    return primask;
}

static void flash_eeprom_irq_restore(uint32_t primask)
{
    /* 恢复进入前状态；调用者原本关着中断时，不能擅自开中断。 */
    if((primask & 1U) == 0U) {
        __enable_irq();
    }
}

static bsp_flash_eeprom_status_t flash_eeprom_erase_page(uint8_t page)
{
    const uint64_t *word =
        (const uint64_t *)(uintptr_t)flash_eeprom_page_address(page);
    fmc_state_enum fmc_state;
    uint32_t primask;
    uint32_t index;

    primask = flash_eeprom_irq_save();
    fmc_unlock();
    flash_eeprom_clear_fmc_flags();
    /* 库函数接收物理“页号”，不是 0x0800xxxx 地址；调用会阻塞等待完成。
     * 整页擦除后还要检查是否全 1。该临界区仅保护硬件操作，不提供任务互斥。 */
    fmc_state = fmc_page_erase(flash_eeprom_page_number(page));
    fmc_lock();
    flash_eeprom_irq_restore(primask);

    if(fmc_state != FMC_READY) {
        return BSP_FLASH_EEPROM_FLASH_ERROR;
    }

    for(index = 0U; index < (MAIN_FLASH_PAGE_SIZE / sizeof(uint64_t)); index++) {
        if(word[index] != FLASH_EEPROM_ERASED_DOUBLEWORD) {
            return BSP_FLASH_EEPROM_VERIFY_ERROR;
        }
    }

    return BSP_FLASH_EEPROM_OK;
}

static bsp_flash_eeprom_status_t flash_eeprom_program_record(
    uint8_t page,
    uint16_t slot,
    const flash_eeprom_record_t *record)
{
    const flash_eeprom_record_t *destination = flash_eeprom_record(page, slot);
    uint32_t address = (uint32_t)(uintptr_t)destination;
    fmc_state_enum fmc_state = FMC_READY;
    uint64_t program_word;
    uint32_t primask;
    uint32_t index;

    primask = flash_eeprom_irq_save();
    fmc_unlock();
    flash_eeprom_clear_fmc_flags();

    /* 先写前 80 字节。commit 仍保持擦除态时，init() 不会接受这条记录。 */
    for(index = 0U; index < FLASH_EEPROM_COMMIT_WORD_INDEX; index++) {
        memcpy(&program_word,
               ((const uint8_t *)record) + (index * sizeof(uint64_t)),
               sizeof(program_word));
        fmc_state = fmc_doubleword_program(
            address + (index * sizeof(uint64_t)), program_word);
        if(fmc_state != FMC_READY) {
            break;
        }
    }

    /* 回读比对前 80 字节全部一致后，才写最后 8 字节 commit。
     * commit 完成前复位，设计上回退到上一条有效快照；完成后再做完整校验。
     * 这种提交顺序不代表低于芯片规定电压时仍能保证 Flash 可靠性。 */
    if((fmc_state == FMC_READY) &&
       (0 == memcmp(destination, record, offsetof(flash_eeprom_record_t, commit)))) {
        program_word = record->commit;
        fmc_state = fmc_doubleword_program(
            address + offsetof(flash_eeprom_record_t, commit),
            program_word);
    } else if(fmc_state == FMC_READY) {
        fmc_state = FMC_UNDEFINEDERR;
    }

    fmc_lock();
    flash_eeprom_irq_restore(primask);

    if(fmc_state != FMC_READY) {
        return (fmc_state == FMC_UNDEFINEDERR) ?
               BSP_FLASH_EEPROM_VERIFY_ERROR :
               BSP_FLASH_EEPROM_FLASH_ERROR;
    }

    if(!flash_eeprom_record_is_valid(destination) ||
       (0 != memcmp(destination->payload, record->payload, sizeof(record->payload)))) {
        return BSP_FLASH_EEPROM_VERIFY_ERROR;
    }

    return BSP_FLASH_EEPROM_OK;
}

bsp_flash_eeprom_status_t bsp_flash_eeprom_init(void)
{
    const flash_eeprom_record_t *latest_record = NULL;
    uint8_t latest_page = FLASH_EEPROM_INVALID_PAGE;
    uint8_t page;
    uint16_t slot;

    memset(g_flash_eeprom_cache, 0xFF, sizeof(g_flash_eeprom_cache));
    g_flash_eeprom_sequence = 0U;
    g_flash_eeprom_active_page = FLASH_EEPROM_INVALID_PAGE;
    g_flash_eeprom_has_data = false;

    /* 扫描两页所有槽，不在空槽/坏记录处提前停止；后面可能还有有效记录。
     * 最新记录由序号决定，不能假定物理地址最大的就是最新。init 不擦写。 */
    for(page = 0U; page < FLASH_EEPROM_PAGE_COUNT; page++) {
        for(slot = 0U; slot < FLASH_EEPROM_RECORDS_PER_PAGE; slot++) {
            const flash_eeprom_record_t *candidate =
                flash_eeprom_record(page, slot);

            if(flash_eeprom_record_is_valid(candidate) &&
               ((latest_record == NULL) ||
                flash_eeprom_sequence_is_newer(candidate->sequence,
                                               latest_record->sequence))) {
                latest_record = candidate;
                latest_page = page;
            }
        }
    }

    if(latest_record != NULL) {
        memcpy(g_flash_eeprom_cache,
               latest_record->payload,
               sizeof(g_flash_eeprom_cache));
        g_flash_eeprom_sequence = latest_record->sequence;
        g_flash_eeprom_active_page = latest_page;
        g_flash_eeprom_has_data = true;
    }

    g_flash_eeprom_initialized = true;
    return g_flash_eeprom_has_data ?
           BSP_FLASH_EEPROM_OK :
           BSP_FLASH_EEPROM_EMPTY;
}

bsp_flash_eeprom_status_t bsp_flash_eeprom_read(uint16_t offset,
                                                 void *data,
                                                 uint16_t length)
{
    if((data == NULL) || (offset > BSP_FLASH_EEPROM_CAPACITY) ||
       (length > (BSP_FLASH_EEPROM_CAPACITY - offset))) {
        return BSP_FLASH_EEPROM_INVALID_ARGUMENT;
    }
    if(!g_flash_eeprom_initialized) {
        return BSP_FLASH_EEPROM_NOT_INITIALIZED;
    }
    if(!g_flash_eeprom_has_data) {
        return BSP_FLASH_EEPROM_EMPTY;
    }

    /* offset 是逻辑字节偏移（不是 Flash 地址）。常规 read 只复制 RAM，
     * 不重新扫描或校验 Flash；重启/显式 init 才从持久化记录重新加载。 */
    memcpy(data, &g_flash_eeprom_cache[offset], length);
    return BSP_FLASH_EEPROM_OK;
}

bsp_flash_eeprom_status_t bsp_flash_eeprom_write(uint16_t offset,
                                                  const void *data,
                                                  uint16_t length)
{
    flash_eeprom_record_t record;
    bsp_flash_eeprom_status_t status;
    uint32_t next_sequence;
    uint8_t target_page;
    uint16_t target_slot;

    if((data == NULL) || (offset > BSP_FLASH_EEPROM_CAPACITY) ||
       (length > (BSP_FLASH_EEPROM_CAPACITY - offset))) {
        return BSP_FLASH_EEPROM_INVALID_ARGUMENT;
    }
    if(!g_flash_eeprom_initialized) {
        return BSP_FLASH_EEPROM_NOT_INITIALIZED;
    }
    if(length == 0U) {
        return BSP_FLASH_EEPROM_OK;
    }
    if(g_flash_eeprom_has_data &&
       (0 == memcmp(&g_flash_eeprom_cache[offset], data, length))) {
        return BSP_FLASH_EEPROM_OK;
    }

    /* 即使只修改一个字节，也先复制旧的 64 字节，再覆盖指定区间。
     * 空存储首次局部写入时，未指定的字节保持 0xFF。
     * 一次 write 保存一个完整快照；两次 write 不构成一个原子事务。 */
    memset(&record, 0xFF, sizeof(record));
    record.magic = FLASH_EEPROM_RECORD_MAGIC;
    record.format_version = FLASH_EEPROM_FORMAT_VERSION;
    record.payload_size = BSP_FLASH_EEPROM_CAPACITY;
    next_sequence = g_flash_eeprom_sequence + 1U;
    /* 序号 0 被保留为无效值，溢出后从 1 继续。 */
    if(next_sequence == 0U) {
        next_sequence = 1U;
    }
    record.sequence = next_sequence;
    memcpy(record.payload, g_flash_eeprom_cache, sizeof(record.payload));
    memcpy(&record.payload[offset], data, length);
    record.crc32 = flash_eeprom_record_crc(&record);
    record.commit = FLASH_EEPROM_COMMIT_MARKER;

    if(g_flash_eeprom_active_page == FLASH_EEPROM_INVALID_PAGE) {
        target_page = 0U;
        target_slot = flash_eeprom_find_erased_slot(target_page);
        if(target_slot == FLASH_EEPROM_INVALID_SLOT) {
            status = flash_eeprom_erase_page(target_page);
            if(status != BSP_FLASH_EEPROM_OK) {
                return status;
            }
            target_slot = 0U;
        }
    } else {
        target_page = g_flash_eeprom_active_page;
        target_slot = flash_eeprom_find_erased_slot(target_page);
        if(target_slot == FLASH_EEPROM_INVALID_SLOT) {
            /* 当前页满：先擦另一页并写新快照，旧活动页保持原样。
             * 新页写成功才成为活动页；旧页等下一次轮换时才回收。
             * 正常连续更新时：页0存序号1~11，页1存12~22，再轮回页0。 */
            target_page = (uint8_t)(1U - g_flash_eeprom_active_page);
            status = flash_eeprom_erase_page(target_page);
            if(status != BSP_FLASH_EEPROM_OK) {
                return status;
            }
            target_slot = 0U;
        }
    }

    status = flash_eeprom_program_record(target_page, target_slot, &record);
    if(status != BSP_FLASH_EEPROM_OK) {
        return status;
    }

    /* 只有编程和校验成功才发布新 RAM 状态，调用失败时不提前更新缓存。 */
    memcpy(g_flash_eeprom_cache, record.payload, sizeof(g_flash_eeprom_cache));
    g_flash_eeprom_sequence = record.sequence;
    g_flash_eeprom_active_page = target_page;
    g_flash_eeprom_has_data = true;
    return BSP_FLASH_EEPROM_OK;
}

bsp_flash_eeprom_status_t bsp_flash_eeprom_format(void)
{
    bsp_flash_eeprom_status_t status;
    uint8_t first_page;
    uint8_t second_page;

    if(!g_flash_eeprom_initialized) {
        return BSP_FLASH_EEPROM_NOT_INITIALIZED;
    }

    /* 先擦非活动页，此阶段仍保留最新记录；开始擦活动页后就可能丢失数据。
     * format 是主动清空，不保证整个格式化过程掉电后仍能恢复旧数据。
     * 日常保存不能先 format，否则会破坏追加记录的寿命和恢复优势。 */
    if(g_flash_eeprom_active_page == FLASH_EEPROM_INVALID_PAGE) {
        first_page = 0U;
    } else {
        first_page = (uint8_t)(1U - g_flash_eeprom_active_page);
    }
    second_page = (uint8_t)(1U - first_page);

    status = flash_eeprom_erase_page(first_page);
    if(status == BSP_FLASH_EEPROM_OK) {
        status = flash_eeprom_erase_page(second_page);
    }

    /* 无论擦除结果如何都重新扫描，使 RAM 状态与 Flash 保持一致。 */
    (void)bsp_flash_eeprom_init();
    return status;
}
