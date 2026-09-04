/**
 * @file bsp_flash_eeprom.c
 * @brief GD32C231 双页日志式 Flash EEPROM 实现。
 */

#include "bsp/bsp_flash_eeprom.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "gd32c2x1.h"
#include "gd32c2x1_fmc.h"

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

    /* 最后的 commit doubleword 必须最后写入。 */
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

    memset(&record, 0xFF, sizeof(record));
    record.magic = FLASH_EEPROM_RECORD_MAGIC;
    record.format_version = FLASH_EEPROM_FORMAT_VERSION;
    record.payload_size = BSP_FLASH_EEPROM_CAPACITY;
    next_sequence = g_flash_eeprom_sequence + 1U;
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

    /* 先擦除非活动页；中途掉电时，当前最新记录仍然保留。 */
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
