#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FURRY_HAL_FLASH_OB_RAW_SIZE_BYTES 0x80
#define FURRY_HAL_FLASH_OB_SIZE_WORDS (FURRY_HAL_FLASH_OB_RAW_SIZE_BYTES / sizeof(uint32_t))
#define FURRY_HAL_FLASH_OB_TOTAL_VALUES (FURRY_HAL_FLASH_OB_SIZE_WORDS / 2)

typedef union {
    uint8_t bytes[FURRY_HAL_FLASH_OB_RAW_SIZE_BYTES];
    union {
        struct {
            uint32_t base;
            uint32_t complementary_value;
        } values;
        uint64_t dword;
    } obs[FURRY_HAL_FLASH_OB_TOTAL_VALUES];
} FurryHalFlashRawOptionByteData;

_Static_assert(
    sizeof(FurryHalFlashRawOptionByteData) == FURRY_HAL_FLASH_OB_RAW_SIZE_BYTES,
    "UpdateManifestOptionByteData size error");

/** Init flash, applying necessary workarounds
 */
void furry_hal_flash_init();

/** Get flash base address
 *
 * @return     pointer to flash base
 */
size_t furry_hal_flash_get_base();

/** Get flash read block size
 *
 * @return     size in bytes
 */
size_t furry_hal_flash_get_read_block_size();

/** Get flash write block size
 *
 * @return     size in bytes
 */
size_t furry_hal_flash_get_write_block_size();

/** Get flash page size
 *
 * @return     size in bytes
 */
size_t furry_hal_flash_get_page_size();

/** Get expected flash cycles count
 *
 * @return     count of erase-write operations
 */
size_t furry_hal_flash_get_cycles_count();

/** Get free flash start address
 *
 * @return     pointer to free region start
 */
const void* furry_hal_flash_get_free_start_address();

/** Get free flash end address
 *
 * @return     pointer to free region end
 */
const void* furry_hal_flash_get_free_end_address();

/** Get first free page start address
 *
 * @return     first free page memory address
 */
size_t furry_hal_flash_get_free_page_start_address();

/** Get free page count
 *
 * @return     free page count
 */
size_t furry_hal_flash_get_free_page_count();

/** Erase Flash
 *
 * @warning    locking operation with critical section, stalls execution
 *
 * @param      page  The page to erase
 */
void furry_hal_flash_erase(uint8_t page);

/** Write double word (64 bits)
 *
 * @warning locking operation with critical section, stalls execution
 *
 * @param      address  destination address, must be double word aligned.
 * @param      data     data to write
 */
void furry_hal_flash_write_dword(size_t address, uint64_t data);

/** Write aligned page data (up to page size)
 *
 * @warning locking operation with critical section, stalls execution
 *
 * @param      address  destination address, must be page aligned.
 * @param      data     data to write
 * @param      length   data length
 */
void furry_hal_flash_program_page(const uint8_t page, const uint8_t* data, uint16_t length);

/** Get flash page number for address
 *
 * @return     page number, -1 for invalid address
 */
int16_t furry_hal_flash_get_page_number(size_t address);

/** Writes OB word, using non-compl. index of register in Flash, OPTION_BYTE_BASE
 *
 * @warning locking operation with critical section, stalls execution
 *
 * @param      word_idx  OB word number
 * @param      value    data to write
 * @return     true if value was written, false for read-only word
 */
bool furry_hal_flash_ob_set_word(size_t word_idx, const uint32_t value);

/** Forces a reload of OB data from flash to registers
 *
 * @warning Initializes system restart
 *
 */
void furry_hal_flash_ob_apply();

/** Get raw OB storage data
 *
 * @return     pointer to read-only data of OB (raw + complementary values)
 */
const FurryHalFlashRawOptionByteData* furry_hal_flash_ob_get_raw_ptr();

#ifdef __cplusplus
}
#endif
