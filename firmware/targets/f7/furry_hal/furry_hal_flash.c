#include <furry_hal_flash.h>
#include <furry_hal_bt.h>
#include <furry_hal_power.h>
#include <furry_hal_cortex.h>
#include <furry.h>
#include <ble/ble.h>
#include <interface/patterns/ble_thread/shci/shci.h>

#include <stm32wbxx.h>

#define TAG "FurryHalFlash"

#define FURRY_HAL_CRITICAL_MSG "Critical flash operation fail"
#define FURRY_HAL_FLASH_READ_BLOCK 8
#define FURRY_HAL_FLASH_WRITE_BLOCK 8
#define FURRY_HAL_FLASH_PAGE_SIZE 4096
#define FURRY_HAL_FLASH_CYCLES_COUNT 10000
#define FURRY_HAL_FLASH_TIMEOUT 1000
#define FURRY_HAL_FLASH_KEY1 0x45670123U
#define FURRY_HAL_FLASH_KEY2 0xCDEF89ABU
#define FURRY_HAL_FLASH_TOTAL_PAGES 256
#define FURRY_HAL_FLASH_SR_ERRORS                                                               \
    (FLASH_SR_OPERR | FLASH_SR_PROGERR | FLASH_SR_WRPERR | FLASH_SR_PGAERR | FLASH_SR_SIZERR | \
     FLASH_SR_PGSERR | FLASH_SR_MISERR | FLASH_SR_FASTERR | FLASH_SR_RDERR | FLASH_SR_OPTVERR)

#define FURRY_HAL_FLASH_OPT_KEY1 0x08192A3B
#define FURRY_HAL_FLASH_OPT_KEY2 0x4C5D6E7F
#define FURRY_HAL_FLASH_OB_TOTAL_WORDS (0x80 / (sizeof(uint32_t) * 2))

/* lib/STM32CubeWB/Projects/P-NUCLEO-WB55.Nucleo/Applications/BLE/BLE_RfWithFlash/Core/Src/flash_driver.c
 * ProcessSingleFlashOperation, quote:
  > In most BLE application, the flash should not be blocked by the CPU2 longer than FLASH_TIMEOUT_VALUE (1000ms)
  > However, it could be that for some marginal application, this time is longer.
  > ... there is no other way than waiting the operation to be completed.
  > If for any reason this test is never passed, this means there is a failure in the system and there is no other
  > way to recover than applying a device reset. 
 */
#define FURRY_HAL_FLASH_C2_LOCK_TIMEOUT_MS 3000u /* 3 seconds */

#define IS_ADDR_ALIGNED_64BITS(__VALUE__) (((__VALUE__)&0x7U) == (0x00UL))
#define IS_FLASH_PROGRAM_ADDRESS(__VALUE__)                                             \
    (((__VALUE__) >= FLASH_BASE) && ((__VALUE__) <= (FLASH_BASE + FLASH_SIZE - 8UL)) && \
     (((__VALUE__) % 8UL) == 0UL))

/* Free flash space borders, exported by linker */
extern const void __free_flash_start__;

size_t furry_hal_flash_get_base() {
    return FLASH_BASE;
}

size_t furry_hal_flash_get_read_block_size() {
    return FURRY_HAL_FLASH_READ_BLOCK;
}

size_t furry_hal_flash_get_write_block_size() {
    return FURRY_HAL_FLASH_WRITE_BLOCK;
}

size_t furry_hal_flash_get_page_size() {
    return FURRY_HAL_FLASH_PAGE_SIZE;
}

size_t furry_hal_flash_get_cycles_count() {
    return FURRY_HAL_FLASH_CYCLES_COUNT;
}

const void* furry_hal_flash_get_free_start_address() {
    return &__free_flash_start__;
}

const void* furry_hal_flash_get_free_end_address() {
    uint32_t sfr_reg_val = READ_REG(FLASH->SFR);
    uint32_t sfsa = (READ_BIT(sfr_reg_val, FLASH_SFR_SFSA) >> FLASH_SFR_SFSA_Pos);
    return (const void*)((sfsa * FURRY_HAL_FLASH_PAGE_SIZE) + FLASH_BASE);
}

size_t furry_hal_flash_get_free_page_start_address() {
    size_t start = (size_t)furry_hal_flash_get_free_start_address();
    size_t page_start = start - start % FURRY_HAL_FLASH_PAGE_SIZE;
    if(page_start != start) {
        page_start += FURRY_HAL_FLASH_PAGE_SIZE;
    }
    return page_start;
}

size_t furry_hal_flash_get_free_page_count() {
    size_t end = (size_t)furry_hal_flash_get_free_end_address();
    size_t page_start = (size_t)furry_hal_flash_get_free_page_start_address();
    return (end - page_start) / FURRY_HAL_FLASH_PAGE_SIZE;
}

void furry_hal_flash_init() {
    /* Errata 2.2.9, Flash OPTVERR flag is always set after system reset */
    // WRITE_REG(FLASH->SR, FLASH_SR_OPTVERR);
    /* Actually, reset all error flags on start */
    if(READ_BIT(FLASH->SR, FURRY_HAL_FLASH_SR_ERRORS)) {
        FURRY_LOG_E(TAG, "FLASH->SR 0x%08lX", FLASH->SR);
        WRITE_REG(FLASH->SR, FURRY_HAL_FLASH_SR_ERRORS);
    }
}

static void furry_hal_flash_unlock() {
    /* verify Flash is locked */
    furry_check(READ_BIT(FLASH->CR, FLASH_CR_LOCK) != 0U);

    /* Authorize the FLASH Registers access */
    WRITE_REG(FLASH->KEYR, FURRY_HAL_FLASH_KEY1);
    __ISB();
    WRITE_REG(FLASH->KEYR, FURRY_HAL_FLASH_KEY2);

    /* verify Flash is unlocked */
    furry_check(READ_BIT(FLASH->CR, FLASH_CR_LOCK) == 0U);
}

static void furry_hal_flash_lock(void) {
    /* verify Flash is unlocked */
    furry_check(READ_BIT(FLASH->CR, FLASH_CR_LOCK) == 0U);

    /* Set the LOCK Bit to lock the FLASH Registers access */
    /* @Note  The lock and unlock procedure is done only using CR registers even from CPU2 */
    SET_BIT(FLASH->CR, FLASH_CR_LOCK);

    /* verify Flash is locked */
    furry_check(READ_BIT(FLASH->CR, FLASH_CR_LOCK) != 0U);
}

static void furry_hal_flash_begin_with_core2(bool erase_flag) {
    furry_hal_power_insomnia_enter();
    /* Take flash controller ownership */
    while(LL_HSEM_1StepLock(HSEM, CFG_HW_FLASH_SEMID) != 0) {
        furry_thread_yield();
    }

    /* Unlock flash operation */
    furry_hal_flash_unlock();

    /* Erase activity notification */
    if(erase_flag) SHCI_C2_FLASH_EraseActivity(ERASE_ACTIVITY_ON);

    /* 64mHz 5us core2 flag protection */
    for(volatile uint32_t i = 0; i < 35; i++)
        ;

    FurryHalCortexTimer timer = furry_hal_cortex_timer_get(FURRY_HAL_FLASH_C2_LOCK_TIMEOUT_MS * 1000);
    while(true) {
        /* Wait till flash controller become usable */
        while(LL_FLASH_IsActiveFlag_OperationSuspended()) {
            furry_check(!furry_hal_cortex_timer_is_expired(timer));
            furry_thread_yield();
        };

        /* Just a little more love */
        taskENTER_CRITICAL();

        /* Actually we already have mutex for it, but specification is specification  */
        if(LL_HSEM_IsSemaphoreLocked(HSEM, CFG_HW_BLOCK_FLASH_REQ_BY_CPU1_SEMID)) {
            taskEXIT_CRITICAL();
            furry_check(!furry_hal_cortex_timer_is_expired(timer));
            furry_thread_yield();
            continue;
        }

        /* Take sempahopre and prevent core2 from anything funky */
        if(LL_HSEM_1StepLock(HSEM, CFG_HW_BLOCK_FLASH_REQ_BY_CPU2_SEMID) != 0) {
            taskEXIT_CRITICAL();
            furry_check(!furry_hal_cortex_timer_is_expired(timer));
            furry_thread_yield();
            continue;
        }

        break;
    }
}

static void furry_hal_flash_begin(bool erase_flag) {
    /* Acquire dangerous ops mutex */
    furry_hal_bt_lock_core2();

    /* If Core2 is running use IPC locking */
    if(furry_hal_bt_is_alive()) {
        furry_hal_flash_begin_with_core2(erase_flag);
    } else {
        furry_hal_flash_unlock();
    }
}

static void furry_hal_flash_end_with_core2(bool erase_flag) {
    /* Funky ops are ok at this point */
    LL_HSEM_ReleaseLock(HSEM, CFG_HW_BLOCK_FLASH_REQ_BY_CPU2_SEMID, 0);

    /* Task switching is ok */
    taskEXIT_CRITICAL();

    /* Doesn't make much sense, does it? */
    while(READ_BIT(FLASH->SR, FLASH_SR_BSY)) {
        furry_thread_yield();
    }

    /* Erase activity over, core2 can continue */
    if(erase_flag) SHCI_C2_FLASH_EraseActivity(ERASE_ACTIVITY_OFF);

    /* Lock flash controller */
    furry_hal_flash_lock();

    /* Release flash controller ownership */
    LL_HSEM_ReleaseLock(HSEM, CFG_HW_FLASH_SEMID, 0);
    furry_hal_power_insomnia_exit();
}

static void furry_hal_flash_end(bool erase_flag) {
    /* If Core2 is running - use IPC locking */
    if(furry_hal_bt_is_alive()) {
        furry_hal_flash_end_with_core2(erase_flag);
    } else {
        furry_hal_flash_lock();
    }

    /* Release dangerous ops mutex */
    furry_hal_bt_unlock_core2();
}

static void furry_hal_flush_cache(void) {
    /* Flush instruction cache  */
    if(READ_BIT(FLASH->ACR, FLASH_ACR_ICEN) == FLASH_ACR_ICEN) {
        /* Disable instruction cache  */
        LL_FLASH_DisableInstCache();
        /* Reset instruction cache */
        LL_FLASH_EnableInstCacheReset();
        LL_FLASH_DisableInstCacheReset();
        /* Enable instruction cache */
        LL_FLASH_EnableInstCache();
    }

    /* Flush data cache */
    if(READ_BIT(FLASH->ACR, FLASH_ACR_DCEN) == FLASH_ACR_DCEN) {
        /* Disable data cache  */
        LL_FLASH_DisableDataCache();
        /* Reset data cache */
        LL_FLASH_EnableDataCacheReset();
        LL_FLASH_DisableDataCacheReset();
        /* Enable data cache */
        LL_FLASH_EnableDataCache();
    }
}

bool furry_hal_flash_wait_last_operation(uint32_t timeout) {
    uint32_t error = 0;

    /* Wait for the FLASH operation to complete by polling on BUSY flag to be reset.
       Even if the FLASH operation fails, the BUSY flag will be reset and an error
       flag will be set */
    FurryHalCortexTimer timer = furry_hal_cortex_timer_get(timeout * 1000);
    while(READ_BIT(FLASH->SR, FLASH_SR_BSY)) {
        if(furry_hal_cortex_timer_is_expired(timer)) {
            return false;
        }
    }

    /* Check FLASH operation error flags */
    error = FLASH->SR;

    /* Check FLASH End of Operation flag */
    if((error & FLASH_SR_EOP) != 0U) {
        /* Clear FLASH End of Operation pending bit */
        CLEAR_BIT(FLASH->SR, FLASH_SR_EOP);
    }

    /* Now update error variable to only error value */
    error &= FURRY_HAL_FLASH_SR_ERRORS;

    furry_check(error == 0);

    /* clear error flags */
    CLEAR_BIT(FLASH->SR, error);

    /* Wait for control register to be written */
    timer = furry_hal_cortex_timer_get(timeout * 1000);
    while(READ_BIT(FLASH->SR, FLASH_SR_CFGBSY)) {
        if(furry_hal_cortex_timer_is_expired(timer)) {
            return false;
        }
    }
    return true;
}

void furry_hal_flash_erase(uint8_t page) {
    furry_hal_flash_begin(true);

    /* Ensure that controller state is valid */
    furry_check(FLASH->SR == 0);

    /* Verify that next operation can be proceed */
    furry_check(furry_hal_flash_wait_last_operation(FURRY_HAL_FLASH_TIMEOUT));

    /* Select page and start operation */
    MODIFY_REG(
        FLASH->CR, FLASH_CR_PNB, ((page << FLASH_CR_PNB_Pos) | FLASH_CR_PER | FLASH_CR_STRT));

    /* Wait for last operation to be completed */
    furry_check(furry_hal_flash_wait_last_operation(FURRY_HAL_FLASH_TIMEOUT));

    /* If operation is completed or interrupted, disable the Page Erase Bit */
    CLEAR_BIT(FLASH->CR, (FLASH_CR_PER | FLASH_CR_PNB));

    /* Flush the caches to be sure of the data consistency */
    furry_hal_flush_cache();

    furry_hal_flash_end(true);
}

static inline void furry_hal_flash_write_dword_internal_nowait(size_t address, uint64_t* data) {
    /* Program first word */
    *(uint32_t*)address = (uint32_t)*data;

    /* Barrier to ensure programming is performed in 2 steps, in right order
     (independently of compiler optimization behavior) */
    __ISB();

    /* Program second word */
    *(uint32_t*)(address + 4U) = (uint32_t)(*data >> 32U);
}

static inline void furry_hal_flash_write_dword_internal(size_t address, uint64_t* data) {
    furry_hal_flash_write_dword_internal_nowait(address, data);

    /* Wait for last operation to be completed */
    furry_check(furry_hal_flash_wait_last_operation(FURRY_HAL_FLASH_TIMEOUT));
}

void furry_hal_flash_write_dword(size_t address, uint64_t data) {
    furry_hal_flash_begin(false);

    /* Ensure that controller state is valid */
    furry_check(FLASH->SR == 0);

    /* Check the parameters */
    furry_check(IS_ADDR_ALIGNED_64BITS(address));
    furry_check(IS_FLASH_PROGRAM_ADDRESS(address));

    /* Set PG bit */
    SET_BIT(FLASH->CR, FLASH_CR_PG);

    /* Do the thing */
    furry_hal_flash_write_dword_internal(address, &data);

    /* If the program operation is completed, disable the PG or FSTPG Bit */
    CLEAR_BIT(FLASH->CR, FLASH_CR_PG);

    furry_hal_flash_end(false);

    /* Wait for last operation to be completed */
    furry_check(furry_hal_flash_wait_last_operation(FURRY_HAL_FLASH_TIMEOUT));
}

static size_t furry_hal_flash_get_page_address(uint8_t page) {
    return furry_hal_flash_get_base() + page * FURRY_HAL_FLASH_PAGE_SIZE;
}

void furry_hal_flash_program_page(const uint8_t page, const uint8_t* data, uint16_t _length) {
    uint16_t length = _length;
    furry_check(length <= FURRY_HAL_FLASH_PAGE_SIZE);

    furry_hal_flash_erase(page);

    furry_hal_flash_begin(false);

    furry_check(furry_hal_flash_wait_last_operation(FURRY_HAL_FLASH_TIMEOUT));

    /* Ensure that controller state is valid */
    furry_check(FLASH->SR == 0);

    size_t page_start_address = furry_hal_flash_get_page_address(page);

    size_t length_written = 0;

    const uint16_t FAST_PROG_BLOCK_SIZE = 512;
    const uint8_t DWORD_PROG_BLOCK_SIZE = 8;

    /* Write as much data as we can in fast mode */
    if(length >= FAST_PROG_BLOCK_SIZE) {
        taskENTER_CRITICAL();
        /* Enable fast flash programming mode */
        SET_BIT(FLASH->CR, FLASH_CR_FSTPG);

        while(length_written < (length / FAST_PROG_BLOCK_SIZE * FAST_PROG_BLOCK_SIZE)) {
            /* No context switch in the middle of the operation */
            furry_hal_flash_write_dword_internal_nowait(
                page_start_address + length_written, (uint64_t*)(data + length_written));
            length_written += DWORD_PROG_BLOCK_SIZE;

            if((length_written % FAST_PROG_BLOCK_SIZE) == 0) {
                /* Wait for block operation to be completed */
                furry_check(furry_hal_flash_wait_last_operation(FURRY_HAL_FLASH_TIMEOUT));
            }
        }
        CLEAR_BIT(FLASH->CR, FLASH_CR_FSTPG);
        taskEXIT_CRITICAL();
    }

    /* Enable regular (dword) programming mode */
    SET_BIT(FLASH->CR, FLASH_CR_PG);
    if((length % FAST_PROG_BLOCK_SIZE) != 0) {
        /* Write tail in regular, dword mode */
        while(length_written < (length / DWORD_PROG_BLOCK_SIZE * DWORD_PROG_BLOCK_SIZE)) {
            furry_hal_flash_write_dword_internal(
                page_start_address + length_written, (uint64_t*)&data[length_written]);
            length_written += DWORD_PROG_BLOCK_SIZE;
        }
    }

    if((length % DWORD_PROG_BLOCK_SIZE) != 0) {
        /* there are more bytes, not fitting into dwords */
        uint64_t tail_data = 0;
        for(int32_t tail_i = 0; tail_i < (length % DWORD_PROG_BLOCK_SIZE); ++tail_i) {
            tail_data |= (((uint64_t)data[length_written + tail_i]) << (tail_i * 8));
        }

        furry_hal_flash_write_dword_internal(page_start_address + length_written, &tail_data);
    }
    /* Disable the PG Bit */
    CLEAR_BIT(FLASH->CR, FLASH_CR_PG);

    furry_hal_flash_end(false);
}

int16_t furry_hal_flash_get_page_number(size_t address) {
    const size_t flash_base = furry_hal_flash_get_base();
    if((address < flash_base) ||
       (address > flash_base + FURRY_HAL_FLASH_TOTAL_PAGES * FURRY_HAL_FLASH_PAGE_SIZE)) {
        return -1;
    }

    return (address - flash_base) / FURRY_HAL_FLASH_PAGE_SIZE;
}

uint32_t furry_hal_flash_ob_get_word(size_t word_idx, bool complementary) {
    furry_check(word_idx <= FURRY_HAL_FLASH_OB_TOTAL_WORDS);
    const uint32_t* ob_data = (const uint32_t*)(OPTION_BYTE_BASE);
    size_t raw_word_idx = word_idx * 2;
    if(complementary) {
        raw_word_idx += 1;
    }
    return ob_data[raw_word_idx];
}

void furry_hal_flash_ob_unlock() {
    furry_check(READ_BIT(FLASH->CR, FLASH_CR_OPTLOCK) != 0U);
    furry_hal_flash_begin(true);
    WRITE_REG(FLASH->OPTKEYR, FURRY_HAL_FLASH_OPT_KEY1);
    __ISB();
    WRITE_REG(FLASH->OPTKEYR, FURRY_HAL_FLASH_OPT_KEY2);
    /* verify OB area is unlocked */
    furry_check(READ_BIT(FLASH->CR, FLASH_CR_OPTLOCK) == 0U);
}

void furry_hal_flash_ob_lock() {
    furry_check(READ_BIT(FLASH->CR, FLASH_CR_OPTLOCK) == 0U);
    SET_BIT(FLASH->CR, FLASH_CR_OPTLOCK);
    furry_hal_flash_end(true);
    furry_check(READ_BIT(FLASH->CR, FLASH_CR_OPTLOCK) != 0U);
}

typedef enum {
    FurryHalFlashObInvalid,
    FurryHalFlashObRegisterUserRead,
    FurryHalFlashObRegisterPCROP1AStart,
    FurryHalFlashObRegisterPCROP1AEnd,
    FurryHalFlashObRegisterWRPA,
    FurryHalFlashObRegisterWRPB,
    FurryHalFlashObRegisterPCROP1BStart,
    FurryHalFlashObRegisterPCROP1BEnd,
    FurryHalFlashObRegisterIPCCMail,
    FurryHalFlashObRegisterSecureFlash,
    FurryHalFlashObRegisterC2Opts,
} FurryHalFlashObRegister;

typedef struct {
    FurryHalFlashObRegister ob_reg;
    uint32_t* ob_register_address;
} FurryHalFlashObMapping;

#define OB_REG_DEF(INDEX, REG) \
    { .ob_reg = INDEX, .ob_register_address = (uint32_t*)(REG) }

static const FurryHalFlashObMapping furry_hal_flash_ob_reg_map[FURRY_HAL_FLASH_OB_TOTAL_WORDS] = {
    OB_REG_DEF(FurryHalFlashObRegisterUserRead, (&FLASH->OPTR)),
    OB_REG_DEF(FurryHalFlashObRegisterPCROP1AStart, (&FLASH->PCROP1ASR)),
    OB_REG_DEF(FurryHalFlashObRegisterPCROP1AEnd, (&FLASH->PCROP1AER)),
    OB_REG_DEF(FurryHalFlashObRegisterWRPA, (&FLASH->WRP1AR)),
    OB_REG_DEF(FurryHalFlashObRegisterWRPB, (&FLASH->WRP1BR)),
    OB_REG_DEF(FurryHalFlashObRegisterPCROP1BStart, (&FLASH->PCROP1BSR)),
    OB_REG_DEF(FurryHalFlashObRegisterPCROP1BEnd, (&FLASH->PCROP1BER)),

    OB_REG_DEF(FurryHalFlashObInvalid, (NULL)),
    OB_REG_DEF(FurryHalFlashObInvalid, (NULL)),
    OB_REG_DEF(FurryHalFlashObInvalid, (NULL)),
    OB_REG_DEF(FurryHalFlashObInvalid, (NULL)),
    OB_REG_DEF(FurryHalFlashObInvalid, (NULL)),
    OB_REG_DEF(FurryHalFlashObInvalid, (NULL)),

    OB_REG_DEF(FurryHalFlashObRegisterIPCCMail, (&FLASH->IPCCBR)),
    OB_REG_DEF(FurryHalFlashObRegisterSecureFlash, (NULL)),
    OB_REG_DEF(FurryHalFlashObRegisterC2Opts, (NULL)),
};
#undef OB_REG_DEF

void furry_hal_flash_ob_apply() {
    furry_hal_flash_ob_unlock();
    /* OBL_LAUNCH: When set to 1, this bit forces the option byte reloading. 
     * It cannot be written if OPTLOCK is set */
    SET_BIT(FLASH->CR, FLASH_CR_OBL_LAUNCH);
    furry_check(furry_hal_flash_wait_last_operation(FURRY_HAL_FLASH_TIMEOUT));
    furry_hal_flash_ob_lock();
}

bool furry_hal_flash_ob_set_word(size_t word_idx, const uint32_t value) {
    furry_check(word_idx < FURRY_HAL_FLASH_OB_TOTAL_WORDS);

    const FurryHalFlashObMapping* reg_def = &furry_hal_flash_ob_reg_map[word_idx];
    if(reg_def->ob_register_address == NULL) {
        FURRY_LOG_E(TAG, "Attempt to set RO OB word %d", word_idx);
        return false;
    }

    FURRY_LOG_W(
        TAG,
        "Setting OB reg %d for word %d (addr 0x%08lX) to 0x%08lX",
        reg_def->ob_reg,
        word_idx,
        (uint32_t)reg_def->ob_register_address,
        value);

    /* 1. Clear OPTLOCK option lock bit with the clearing sequence */
    furry_hal_flash_ob_unlock();

    /* 2. Write the desired options value in the options registers */
    *reg_def->ob_register_address = value;

    /* 3. Check that no Flash memory operation is on going by checking the BSY && PESD */
    furry_check(furry_hal_flash_wait_last_operation(FURRY_HAL_FLASH_TIMEOUT));
    while(LL_FLASH_IsActiveFlag_OperationSuspended()) {
        furry_thread_yield();
    };

    /* 4. Set the Options start bit OPTSTRT */
    SET_BIT(FLASH->CR, FLASH_CR_OPTSTRT);

    /* 5. Wait for the BSY bit to be cleared. */
    furry_check(furry_hal_flash_wait_last_operation(FURRY_HAL_FLASH_TIMEOUT));
    furry_hal_flash_ob_lock();
    return true;
}

const FurryHalFlashRawOptionByteData* furry_hal_flash_ob_get_raw_ptr() {
    return (const FurryHalFlashRawOptionByteData*)OPTION_BYTE_BASE;
}
